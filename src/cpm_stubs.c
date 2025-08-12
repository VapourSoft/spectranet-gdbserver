#include <stdint.h>
#include <stdio.h>
#include "utils.h"

#ifdef TARGET_PCW_DART

// CP/M-safe putchar: z88dk maps putchar() -> putchar_fastcall(), so implement that symbol
int putchar_fastcall(int c) __z88dk_fastcall {
    if (c == '\n') {
        fputc('\r', stdout);
    }
    fputc(c, stdout);
    fflush(stdout);
    return c & 0xff;
}

void print42(const char* text) {
    while (*text) putchar((unsigned char)*text++);
}

void clear42(void) {
    // Simple clear: CRLF a few times (CP/M has no standard clear)
    for (uint8_t i = 0; i < 5; ++i) { putchar('\r'); putchar('\n'); }
}

void set_trap(void* trap_handler) {
    (void)trap_handler; // no-op for CP/M test harness
}

void reset_trap(void) {
    // no-op
}

#endif
