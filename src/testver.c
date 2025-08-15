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


// Call BDOS function 60 (CALL RSX) with function code 1 (install/init)
void call_rsx_install(void) {
    // BDOS 60: C=60, DE=function code, HL=parameter block (optional)
    // We'll use function code 1 for install/init, HL=0
    __asm
        ld c,60     ; BDOS function 60
        ld de,1     ; my init subfunction
        ld hl,0     ; no prams
        call 5
    __endasm;
}

int main(void) {
    printf("CP/M Plus RSX GDB Server v1.0\n");

    // Call RSX install/init via BDOS 60
    printf("Initialising...\r\n");
    call_rsx_install();

    printf("RSX Installed\n");
    return 0;
}
