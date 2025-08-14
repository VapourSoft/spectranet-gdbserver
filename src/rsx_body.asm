; ZDBG RSX body for CP/M Plus (PRL generation)
; This file is assembled twice with different ORGs to generate
; image.0 (template @ 0x0201) and image.1 (real @ 0x0100) for makeprl.
; Keep any absolute 16-bit addresses as real words so the relocation
; bitmap can detect them by difference of +0x0101.
;
; NOTE: Do NOT perform any self-relocation or BDOS patching here.
; GENCOM / CP/M loader will allocate and chain the RSX via the header.
;
; Layout (must start with RSX header expected by CP/M Plus):
; RSXTOP: 6 bytes workspace
; ICEPT : JP entry for intercept (BDOS calls enter here when resident)
; FDOS  : JP previous RSX / BDOS (filled by loader)
;  WORD  : 0x0007 (RSX flag) per CP/M Plus conventions (placeholder)
; DELFLG: Two bytes deletion flags
; RNAME : 8-char name + 3 flag bytes
;
; After the header we place a simple execution entry that prints a message
; (helpful when testing the PRL as a transient too) then returns.
;
; Public symbols (if later linked with other modules): rsx_start

; Select origin via TEMPLATE macro (z80asm: pass -DTEMPLATE for template build)



IF TEMPLATE
    ORG 0x0000
    ;ORG 0x0000
ELSE
    ORG 0x0100
ENDIF

    EXTERN _rsx_c_banner

    

    PRINT_STRING equ 9

    PUBLIC rsx_start
    PUBLIC ftest
    PUBLIC NEXT

rsx_start:
RSXTOP:   
          db 0,0,0
          db 0,0,0           ; loader puts serial here
ICEPT:    
          jp ftest       ; Intercept entry for BDOS chain
NEXT:     
          db 0xc3            ; jump instruction 
          dw 0               ; to next module in line (if this rsx makes BDOS calls then they should be chained here - i.e. jmp next)
PREVIOUS: 
          dw 0               ; the address of the previous module or 0x0005 if its the firt in line
DELFLG:   
          db 0               ; RSX remove flag -1 (0xFF) if it should be removed (if set to 0 then it can be set to -1 when it has terminated to remove the RSX)
NONBANK:
          db 0               ; Non-bank
RNAME:    
          db "ZDBG1234"      ; 8-char name
LOADER:
          db 0               ; loader flag
          db 0,0             ; flag / version bytes (unused for now)

; Transient execution entry when run as a COM (GENCOM may execute init code)
ftest:
    mov a,c
    cpi 12
    jz  handle
    jmp NEXT
handle:
    lxi h,0
    dad sp                    ; save stack pointer
    shld ret_stack
    lxi sp,loc_stack

    call _rsx_c_banner

    mvi c,PRINT_STRING
    lxi d,msg
    call NEXT
    lhld ret_stack            ; restore stack
    sphl
    lxi h,0x35
    ret

; Simple intercept: immediately chain to previous RSX / BDOS.
rsx_icept:
    jp NEXT      ; FDOS contains jp <prev> so this jumps through to it

msg: defb "ZDBG RSX body$"

ret_stack: dw 0
           ds 32 ; 16 level stack

loc_stack:


; (Optional) future: RST 08 handler or private functions would go after this.

