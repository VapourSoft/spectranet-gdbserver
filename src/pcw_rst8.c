#include <stdint.h>
#include "state.h"
#include "pcw_rst8.h"
#include "server.h"
#include "pcw_dart.h"
#include "utils.h"
#include "z80_decode.h"

extern struct gdbserver_state_t gdbserver_state;

// Assembly writes current SP to _rst8_sp_copy before C is invoked
extern uint16_t rst8_sp_copy; // storage in pcw_rst8.asm

/*
 Stack (lowest address first) when rst8_c_trap runs:
   0: IY
   1: IX
   2: HL
   3: DE
   4: BC
   5: AF
   6: ret_addr (address AFTER RST 08 byte)
*/
extern void printS(const char* str) __z88dk_fastcall ;



void rst8_c_trap(void)
{
  // Print debug message to the screen using BDOS call
  printS("\r\n\r\n[RST08!]\r\n$");

  char hexbuf[5];
  // Stack layout after pushes (each push 2 bytes):
  // push af, bc, de, hl, ix, iy  (last pushed at lowest address)
  // Memory ascending: IY, IX, HL, DE, BC, AF, ret_addr
  // Total pushed: 12 bytes registers + 2 ret = 14?  Actually ret was on stack before pushes.
  // After pushes SP_new = original_SP - 12. rst8_sp_copy holds original SP (before pushes).
  uint16_t *base = (uint16_t*)(rst8_sp_copy - 12);
  uint16_t ret_addr = base[6];

  // User SP before our pushes = (SP_new + 12) = rst8_sp_copy
  uint16_t user_sp = rst8_sp_copy;


  int trapped_breakpoint = -1;
  int temp_breakpoint_hit = 0;

  // Handle temporary (single-step) breakpoint
  if (gdbserver_state.temporary_breakpoint.address &&
      gdbserver_state.temporary_breakpoint.address == (ret_addr - 1)) {
    // We've hit the temp breakpoint for single-step

    // Restore original instruction
    *(uint8_t*)(gdbserver_state.temporary_breakpoint.address) = gdbserver_state.temporary_breakpoint.original_instruction;

    //Logging
    printS("[SS BP] $");
    char_to_hex(hexbuf, (gdbserver_state.temporary_breakpoint.address >> 8) & 0xFF);
    char_to_hex(hexbuf + 2, gdbserver_state.temporary_breakpoint.address & 0xFF);
    hexbuf[4] = '$';
    printS(hexbuf);      
    printS("\r\n$");

    // Clear temp breakpoint state
    gdbserver_state.temporary_breakpoint.address = 0;
    // Flag that we hit the temp breakpoint
    temp_breakpoint_hit = 1;

  } else {
    // Check for real breakpoint
    for (int i = 0; i < MAX_BREAKPOINTS_COUNT; i++) {
      if (gdbserver_state.breakpoints[i].address == (ret_addr - 1)) {
        // Restore original instruction
        *(uint8_t*)(gdbserver_state.breakpoints[i].address) = gdbserver_state.breakpoints[i].original_instruction;
        trapped_breakpoint = i;
        printS("[Breakpoint] $");

        //Logging
        char_to_hex(hexbuf, ((ret_addr - 1) >> 8) & 0xFF);
        char_to_hex(hexbuf + 2, (ret_addr - 1) & 0xFF);
        hexbuf[4] = '$';
        printS(hexbuf);      
        printS("\r\n$");

        break;
      }
    }
  }


  // Set/clear break hit flag
  gdbserver_state.trap_flags = (gdbserver_state.trap_flags & ~TRAP_FLAG_BREAK_HIT) |
    ((trapped_breakpoint >= 0 || temp_breakpoint_hit) ? TRAP_FLAG_BREAK_HIT : 0);

  gdbserver_state.registers[REGISTERS_PC] = (uint16_t)(ret_addr - 1); // address of breakpoint instr (RST 08)
  gdbserver_state.registers[REGISTERS_SP] = user_sp;                  // SP at trap (points to ret_addr)

  //read registers from stack order they are saved on stack is important !! see pcw_rst8.asm
  gdbserver_state.registers[REGISTERS_IY] = base[0];
  gdbserver_state.registers[REGISTERS_IX] = base[1];  
  gdbserver_state.registers[REGISTERS_HL] = base[2];
  gdbserver_state.registers[REGISTERS_DE] = base[3];
  gdbserver_state.registers[REGISTERS_BC] = base[4];
  gdbserver_state.registers[REGISTERS_AF] = base[5];

  // Always tell GDB we've stopped (breakpoint or step)
  server_write_packet("T05thread:p01.01;");

  // Enter GDB server main loop (wait for GDB connection/commands)
  while (server_read_data()) {
    // loop until GDB says continue/step
  }


  // Write back modified registers to stack so they are restored by the pops/ret in rst8_entry
  base[0] = gdbserver_state.registers[REGISTERS_IY];
  base[1] = gdbserver_state.registers[REGISTERS_IX];
  base[2] = gdbserver_state.registers[REGISTERS_HL];
  base[3] = gdbserver_state.registers[REGISTERS_DE];
  base[4] = gdbserver_state.registers[REGISTERS_BC];
  base[5] = gdbserver_state.registers[REGISTERS_AF];

  // TODO: Shouldnt we restore the RST 08 if the breakpoint is still there, regardless of PC?
  // After GDB loop: if we stopped at a breakpoint and PC is still at the breakpoint address, restore the RST 08
  if (trapped_breakpoint >= 0 &&
      gdbserver_state.registers[REGISTERS_PC] == gdbserver_state.breakpoints[trapped_breakpoint].address) {
      *(uint8_t*)(gdbserver_state.breakpoints[trapped_breakpoint].address) = 0xCF; // RST 08h
  }

  // If step requested set temp breakpoint at next instruction
  if ((gdbserver_state.trap_flags & TRAP_FLAG_STEP_INSTRUCTION) /*&& !temp_breakpoint_hit */) {
    // Use robust Z80 instruction decoder to find next instruction address
    uint16_t next_addr = calculateStep();
    gdbserver_state.temporary_breakpoint.address = next_addr;
    gdbserver_state.temporary_breakpoint.original_instruction = *(uint8_t*)next_addr;
    *(uint8_t*)next_addr = 0xCF; // RST 08h
    printS("[SetStepBP] $");
    char_to_hex(hexbuf, (next_addr >> 8) & 0xFF);
    char_to_hex(hexbuf + 2, next_addr & 0xFF);
    hexbuf[4] = '$';
    printS(hexbuf);
    printS("\r\n$");
    // We've handled the step request by installing the temporary breakpoint; clear the request flag
    gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_STEP_INSTRUCTION;
  }


  // Determine how to resume: use trapped_breakpoint or temp_breakpoint state
  uint16_t resume_addr = 0;
  if (gdbserver_state.registers[REGISTERS_PC] != (uint16_t)(ret_addr - 1)) {
    // User changed PC in GDB, resume at new PC
    printS("[User Changed PC] $");
    resume_addr = gdbserver_state.registers[REGISTERS_PC];
  } else if (trapped_breakpoint >= 0 || temp_breakpoint_hit) {
    // Resume at the breakpoint address (original instruction restored)
    printS("[Resume at BP address] $");
    resume_addr = (uint16_t)(ret_addr - 1);
  } else {
    // Explicit RST 08 in user code: resume after RST 08
    printS("[Explicit RST 08 resume after RST 08] $");    
    resume_addr = ret_addr;
  }
  base[6] = resume_addr;

  // Debug print: show return address and PC before returning (no stdlib, use char_to_hex)
  printS(" ret=$");
  char_to_hex(hexbuf, (base[6] >> 8) & 0xFF);
  char_to_hex(hexbuf + 2, base[6] & 0xFF);
  hexbuf[4] = '$';
  printS(hexbuf);
  printS(" pc=$");
  char_to_hex(hexbuf, (gdbserver_state.registers[REGISTERS_PC] >> 8) & 0xFF);
  char_to_hex(hexbuf + 2, gdbserver_state.registers[REGISTERS_PC] & 0xFF);
  hexbuf[4] = '$';
  printS(hexbuf);
  printS("\r\n$");
}
