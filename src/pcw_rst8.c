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
void rst8_c_trap(void)
{
  // Stack layout after pushes (SP_new = rst8_sp_copy - 12):
  // [0] IY, [1] IX, [2] HL, [3] DE, [4] BC, [5] AF, [6] ret_addr
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

  gdbserver_state.trap_flags |= TRAP_FLAG_BREAK_HIT;
}
