#include <stdint.h>
#include "state.h"
#include "pcw_rst8.h"

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
  gdbserver_state.registers[REGISTERS_HL] = base[2];
  gdbserver_state.registers[REGISTERS_DE] = base[3];
  gdbserver_state.registers[REGISTERS_BC] = base[4];
  gdbserver_state.registers[REGISTERS_AF] = base[5];
  gdbserver_state.registers[REGISTERS_IX] = base[1];
  gdbserver_state.registers[REGISTERS_IY] = base[0];

  // Print debug message to the screen using BDOS call
  printS("[RST08!]\r\n$");

  gdbserver_state.trap_flags |= TRAP_FLAG_BREAK_HIT;
}
