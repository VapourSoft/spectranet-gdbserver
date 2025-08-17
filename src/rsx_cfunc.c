
// Minimal C function for RSX linkage test - NOTE cant use library functions !!!!


// With __z88dk_fastcall, the first argument is passed in the HL register (not on the stack),
// as this is not __naked function, the C compiler will generate a prologue and epilogue
// to save and restore registers, so we can use the stack and other registers as needed.    


//Must be $ terminated!!!!
void printS(const char* str) __z88dk_fastcall 
{
    str; // suppress unused warning
    __asm
        EXTERN NEXT 
        PRINT_STRING equ 9

        ld  de,hl   ; DE = str        

        mvi c,PRINT_STRING

        call NEXT  ; call the BDOS function
        
        
    __endasm;

}


void rsx_c_banner(void)  {
/*    __asm
        EXTERN NEXT 
        PRINT_STRING equ 9
        mvi c,PRINT_STRING
        lxi d,msg
        call NEXT
        ret
        msg: defb "C FUMCTION body$"
    __endasm;*/

    printS("\nRSX C function body\n$");
       
}
