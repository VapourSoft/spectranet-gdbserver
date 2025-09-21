// PCW DART minimal polled backend (SDCC/ASxxxx compatible)
#include <stdint.h>

void dart_putc(const char c)  __z88dk_fastcall 
{
	c; // suppress unused warning
    __asm
        EXTERN SENDBYTE 

        ld  a,l   
        call SENDBYTE

		;for debugging ...
        ;EXTERN NEXT 
        ;ld  e,a
        ;mvi c,2
        ;call NEXT  ; call the BDOS function
        
    __endasm;
}

uint8_t dart_getc(void)  __z88dk_fastcall 
{
    __asm
        EXTERN RECIEVEBYTE 

        call RECIEVEBYTE

		;for debugging 
		;PUSH AF
        ;EXTERN NEXT 
        ;ld  e,a
        ;mvi c,2
        ;call NEXT  ; call the BDOS function
		;POP AF

		//retunr the char in A
		ld l,a

	__endasm;

}
