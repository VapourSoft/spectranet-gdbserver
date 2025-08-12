#include <stdint.h>
#include <stddef.h>
#include "server.h"
#include "utils.h"
#include "state.h"

// Minimal CP/M console stubs to keep output calls safe if CRT hasn't provided them
#ifndef TARGET_PCW_DART
#define TARGET_PCW_DART 1
#endif

// Simple CP/M entry for testing the serial GDB server
int main(void) {
    clear42();
    print42("CP/M GDB server (PCW DART)\n");

    #ifdef DEBUG
    print42("Debug mode enabled\n");
    #else
    print42("Debug mode NOT enabled\n");
    #endif

    if (server_init()) {
        print42("init failed\n");
        return 1;
    }
    if (server_listen()) {
        print42("listen failed\n");
        return 2;
    }

    // Basic packet echo: wait for a single packet and respond with OK
    for(;;) {
        if (gdbserver_state.trap_flags & TRAP_FLAG_BREAK_HIT) {
            server_write_packet("T05");
            gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_BREAK_HIT;
        }
        if (!server_read_data()) break;
    }

    print42("done\n");
    return 0;
}
