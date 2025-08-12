#include "server.h"
#include "utils.h"
#include "state.h"
#include "pcw_dart.h"
#include <stddef.h>

#ifndef POLLIN
#define POLLIN 1
#endif

extern void gdbserver_trap();

void modulecall()
{
    clear42();
    print42("gdbserver by @desertkun\n");

    if (server_init())
    {
        return;
    }

    // reset breaking
    gdbserver_state.trap_flags = 0;

    struct breakpoint_t* trapped_breakpoint = NULL;
    uint16_t* pc = &gdbserver_state.registers[REGISTERS_PC];

    if (gdbserver_state.client_socket)
    {
        print42("execution stopped\n");

        if (gdbserver_state.temporary_breakpoint.address == (*pc - 1))
        {
            // we've hit temp breakpoint, original instruction
            // note, we do not put RST08 back, because it is temporary
            *(uint8_t*)gdbserver_state.temporary_breakpoint.address =
                gdbserver_state.temporary_breakpoint.original_instruction;
            // unwind RST08
            (*pc)--;
        }
        else
        {
            for (uint8_t i = 0; i < MAX_BREAKPOINTS_COUNT; i++)
            {
                struct breakpoint_t* b = &gdbserver_state.breakpoints[i];
                if (b->address == (*pc - 1)) // offset for RST08
                {
                    trapped_breakpoint = b;

                    // restore original instruction
                    *(uint8_t*)b->address = b->original_instruction;
                    break;
                }
            }
        }

        // report we have trapped
        server_write_packet("T05thread:p01.01;");
    }
    else
    {
        if (server_listen())
        {
            return;
        }
    }

    if (trapped_breakpoint)
    {
        // unwind RST08
        (*pc)--;
    }

    while (1)
    {
    // Serial has no poll; emulate POLLIN when data arrives
    switch (dart_rx_ready() ? POLLIN : 0)
        {
            case POLLIN:
            {
                if (server_read_data())
                {
                    continue;
                }
                else
                {
                    goto done;
                }
            }
            default:
            {
        // idle spin
        continue;
            }
        }
    }

done:
    print42("resuming execution\n");

    if (trapped_breakpoint && trapped_breakpoint->address == gdbserver_state.registers[REGISTERS_PC])
    {
        // we have to restore RST08 on the breakpoint
        gdbserver_state.trap_flags |= TRAP_FLAG_RESTORE_RST08H;
    }

    if (gdbserver_state.trap_flags)
    {
        gdbserver_state.trap_handler.page = 0xFF;
        gdbserver_state.trap_handler.address = *pc;
        gdbserver_state.trap_handler.next_address = 0x0000;
        gdbserver_state.trap_handler.handler = (uint16_t)gdbserver_trap;

        set_trap(&gdbserver_state.trap_handler);
    }
}
