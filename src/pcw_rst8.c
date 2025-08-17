#include <stdint.h>
#include "state.h"
#include "pcw_rst8.h"
#include "server.h"
#include "pcw_dart.h"
#include "utils.h"

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
  // Stack layout after pushes (each push 2 bytes):
  // push af, bc, de, hl, ix, iy  (last pushed at lowest address)
  // Memory ascending: IY, IX, HL, DE, BC, AF, ret_addr
  // Total pushed: 12 bytes registers + 2 ret = 14?  Actually ret was on stack before pushes.
  // After pushes SP_new = original_SP - 12. rst8_sp_copy holds original SP (before pushes).
  uint16_t *base = (uint16_t*)(rst8_sp_copy - 12);
  uint16_t ret_addr = base[6];

  // User SP before our pushes = (SP_new + 12) = rst8_sp_copy
  uint16_t user_sp = rst8_sp_copy;

  // Restore original instruction at breakpoint immediately (Spectranet logic) - IF we hit a breakpoint
  int trapped_breakpoint = -1;
  for (int i = 0; i < MAX_BREAKPOINTS_COUNT; i++) {
    if (gdbserver_state.breakpoints[i].address == (ret_addr - 1)) {
      *(uint8_t*)(gdbserver_state.breakpoints[i].address) = gdbserver_state.breakpoints[i].original_instruction;
      trapped_breakpoint = i;
      printS("[Breakpoint]\r\n$");
      break;
    }
  }

  gdbserver_state.trap_flags = (gdbserver_state.trap_flags & ~TRAP_FLAG_BREAK_HIT) |
    ((trapped_breakpoint >= 0) ? TRAP_FLAG_BREAK_HIT : 0);

  gdbserver_state.registers[REGISTERS_PC] = (uint16_t)(ret_addr - 1); // address of breakpoint instr (RST 08)
  gdbserver_state.registers[REGISTERS_SP] = user_sp;                  // SP at trap (points to ret_addr)

  //read registers from stack order they are saved on stack is important !! see pcw_rst8.asm
  gdbserver_state.registers[REGISTERS_IY] = base[0];
  gdbserver_state.registers[REGISTERS_IX] = base[1];  
  gdbserver_state.registers[REGISTERS_HL] = base[2];
  gdbserver_state.registers[REGISTERS_DE] = base[3];
  gdbserver_state.registers[REGISTERS_BC] = base[4];
  gdbserver_state.registers[REGISTERS_AF] = base[5];

  // Print debug message to the screen using BDOS call
  printS("[RST08!]\r\n$");

  //tell any client we have stopped
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

    // After GDB loop: if we stopped at a breakpoint and PC is still at the breakpoint address, set restore flag
  if ((gdbserver_state.trap_flags & TRAP_FLAG_BREAK_HIT) &&
      trapped_breakpoint >= 0 &&
      gdbserver_state.registers[REGISTERS_PC] == gdbserver_state.breakpoints[trapped_breakpoint].address) {
    gdbserver_state.trap_flags |= TRAP_FLAG_RESTORE_RST08H;
  }

  // Determine how to resume: use trapped_breakpoint state
  uint16_t resume_addr = 0;
  if (gdbserver_state.registers[REGISTERS_PC] != (uint16_t)(ret_addr - 1)) {
    // User changed PC in GDB, resume at new PC
    resume_addr = gdbserver_state.registers[REGISTERS_PC];
  } else if (trapped_breakpoint >= 0) {
    // Resume at the breakpoint address (original instruction restored)
    resume_addr = (uint16_t)(ret_addr - 1);
  } else {
    // Explicit RST 08 in user code: resume after RST 08
    resume_addr = ret_addr;
  }
  base[6] = resume_addr;



  // Debug print: show return address and PC before returning (no stdlib, use char_to_hex)
  char hexbuf[5];
  printS("[DBG] ret=$");
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
