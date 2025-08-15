#include <stdio.h>

void main() {
    // Trigger RST 08 (software interrupt)
    __asm__("rst 0x08");
    // Print message to show program ran
    puts("RSXCHK: RST 08 called\r\n");
}
