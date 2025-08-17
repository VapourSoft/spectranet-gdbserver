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

  gdbserver_state.trap_flags |= TRAP_FLAG_BREAK_HIT;

  /*while (1) {
      char c = dart_getc();
      dart_putc(c);
  }*/

  // Enter GDB server main loop (wait for GDB connection/commands)
  while (server_read_data()); 
  
  /* {
    int result = server_read_data();
    // Exit loop if GDB sends continue/step (trap_flags cleared)
    if (!(gdbserver_state.trap_flags & TRAP_FLAG_BREAK_HIT))
      break;
  } */

  // Write back modified registers to stack so they are restored by the pops/ret in rst8_entry
  // base[0] = IY, base[1] = IX, base[2] = HL, base[3] = DE, base[4] = BC, base[5] = AF, base[6] = ret_addr
  base[0] = gdbserver_state.registers[REGISTERS_IY];
  base[1] = gdbserver_state.registers[REGISTERS_IX];
  base[2] = gdbserver_state.registers[REGISTERS_HL];
  base[3] = gdbserver_state.registers[REGISTERS_DE];
  base[4] = gdbserver_state.registers[REGISTERS_BC];
  base[5] = gdbserver_state.registers[REGISTERS_AF];
  // If PC was changed, update return address (ret_addr = PC+1, since ret will pop this)
  if (gdbserver_state.registers[REGISTERS_PC] != (uint16_t)(ret_addr - 1)) {
    base[6] = gdbserver_state.registers[REGISTERS_PC] + 1;
  }

  // Debug print: show return address and PC before returning (no stdlib, use char_to_hex)
  char hexbuf[5];
  hexbuf[0] = 'A';
  hexbuf[1] = 'B';
  hexbuf[2] = '$';
  printS("[DBG] ret=$");
  char_to_hex(hexbuf, (base[6] >> 8) & 0xFF);
  char_to_hex(hexbuf + 2, base[6] & 0xFF);
  hexbuf[4] = '$'; // BDOS string terminator
  printS(hexbuf);

  //char_to_hex(hexbuf, (gdbserver_state.registers[REGISTERS_PC] >> 8) & 0xFF);
  //char_to_hex(hexbuf + 2, gdbserver_state.registers[REGISTERS_PC] & 0xFF);
  //hexbuf[4] = '$';
  printS(hexbuf);
  printS("\r\n$");
}
