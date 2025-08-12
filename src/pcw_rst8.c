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
  uint16_t *frame = (uint16_t*)rst8_sp_copy;

    uint16_t ret_addr = frame[6];
    uint16_t user_sp  = (uint16_t)(&frame[7]); // SP before our pushes

    gdbserver_state.registers[REGISTERS_PC] = (uint16_t)(ret_addr - 1);
    gdbserver_state.registers[REGISTERS_SP] = user_sp;
    gdbserver_state.registers[REGISTERS_HL] = frame[2];
    gdbserver_state.registers[REGISTERS_DE] = frame[3];
    gdbserver_state.registers[REGISTERS_BC] = frame[4];
    gdbserver_state.registers[REGISTERS_AF] = frame[5];

    gdbserver_state.trap_flags |= TRAP_FLAG_BREAK_HIT;
}
