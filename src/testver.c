/* Simple CP/M COM program to call BDOS function 12 (Get Version Number)
 * and print the returned version word. Intended to exercise the RSX
 * intercept test handler you added for BDOS function 12.
 *
 * Build (z88dk example):
 *   zcc +cpm -create-app -compiler=sccz80 testver.c -o testver
 * Produces: TESTVER.COM
 */
#include <stdio.h>


// Global storage for inline assembly to deposit HL result. 
static volatile unsigned int bdos_version_word;

unsigned int bdos_get_version(void) {
    // BDOS function 12: C=12, CALL 5, returns version in HL.
       //We store HL into bdos_version_word and return it. 
    __asm
        ld c,12
        call 5
        ld (_bdos_version_word),hl
    __endasm; 
    return bdos_version_word; 
}

int main(void) {

    printf("Starting...\n");

    unsigned int ver = bdos_get_version();
    unsigned char major = (ver >> 8) & 0xff;  
    unsigned char minor = ver & 0xff;         

    // Print both decoded and raw forms. Use CRLF for CP/M console. 
    printf("BDOS version: %u.%u (raw 0x%04X)\r\n", (unsigned)major, (unsigned)minor, ver);

    printf("...Done\n");
    return 0; 
}
