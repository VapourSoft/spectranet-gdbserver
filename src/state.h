#ifndef __STATE_H
#define __STATE_H

#include <stdint.h>

#define MAX_BREAKPOINTS_COUNT (8)
// Order must match XML feature description
typedef enum {
    REGISTERS_SP = 0,
    REGISTERS_PC,
    REGISTERS_HL,
    REGISTERS_DE,
    REGISTERS_BC,
    REGISTERS_AF,
    REGISTERS_IX,
    REGISTERS_IY,
    SLOT_0,
    SLOT_1,
    SLOT_2,
    SLOT_3,
    REGISTERS_COUNT
} register_index_t;


struct breakpoint_t {
    uint16_t address;
    uint8_t original_instruction;
};

/*
 * Beware, only 1021 bytes are available
 * Offsets of these are crucial
 */
struct gdbserver_state_t
{
    // Offset: 0
    uint8_t trap_flags;            // (1 byte)

    // Offset: 1
    struct breakpoint_t temporary_breakpoint;
    
    // Offsets of these is not important
    struct breakpoint_t breakpoints[MAX_BREAKPOINTS_COUNT];
    uint16_t registers[REGISTERS_COUNT];      // REGISTERS_COUNT * 2 bytes
    uint8_t buffer[128];
    uint8_t w_buffer[128];
    uint8_t no_ack_mode;
};

#define TRAP_FLAG_RESTORE_RST08H (0x01)
#define TRAP_FLAG_STEP_INSTRUCTION (0x02)
#define TRAP_FLAG_BREAK_HIT (0x04)
#define TRAP_FLAG_FORCE_ADDRESS (0x08) // use temporary breakpoint address as next instruction address instead of calculating it

extern struct gdbserver_state_t gdbserver_state;

#endif