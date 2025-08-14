// Minimal C function for RSX linkage test
void rsx_c_banner(void)  {
    __asm

        EXTERN NEXT 

        PRINT_STRING equ 9

        mvi c,PRINT_STRING
        lxi d,msg
        call NEXT

;        ld e, 'C'
;        ld c, 2
;        call 5
        
        ret

        msg: defb "C FUMCTION body$"

        __endasm;
}
