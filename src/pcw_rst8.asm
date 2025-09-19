; Minimal RST 08 hook for Amstrad PCW (CP/M)
; Captures registers and calls C helper; no Spectranet dependencies.

    PUBLIC _rst8_install
    PUBLIC _rst8_restore
    PUBLIC rst8_entry
    PUBLIC im1_entry
    PUBLIC orig_rst38_bytes

    EXTERN _rst8_c_trap
    EXTERN _rst8_sp_copy
    EXTERN _our_sp_base
    EXTERN _dart_putc          ; serial output helper (C symbol dart_putc)
    EXTERN _enable_serial_interrupt ; flag to enable/disable serial interrupt checking in C code

    DART_DATA  equ  0xE0
    DART_CTRL  equ  0xE1

    EXTERN _gdbserver_state
    EXTERN _wasInterrupt

    defc gdbserver_trap_handler = _gdbserver_state + 7
    defc gdbserver_trap_flags = gdbserver_trap_handler + 1
    defc temporary_breakpoint = gdbserver_trap_flags + 1 
    defc original_instruction = temporary_breakpoint + 2

    defc TRAP_FLAG_RESTORE_RST08H   =  0x01
    defc TRAP_FLAG_STEP_INSTRUCTION =  0x02
    defc TRAP_FLAG_BREAK_HIT        =  0x04

    defc STEPPING_MASK = TRAP_FLAG_RESTORE_RST08H + TRAP_FLAG_STEP_INSTRUCTION

    ; Use z88dk standard section names so code isn't lost / zeroed
    SECTION bss_user

safe_stack: 
    defs 256         ; 256 bytes for private stack
safe_stack_top:              ; label for top of stack (highest address)

orig_rst8_bytes:
    defs 8              ; storage for original 8 bytes at 0008h

orig_rst38_bytes:
    defs 8              ; storage for original 8 bytes at 0038h (IM1)

_our_sp_base:
    defs 2              ; storage for our private stack base (so C can get at our pushed regs ) )

_rst8_sp_copy:
    defs 2              ; space for saved SP

    SECTION code_user

; Install: copy original bytes then patch with JP rst8_entry
_rst8_install:
    ; Hook RST 08
    ld hl,0x0008
    ld de,orig_rst8_bytes
    ld bc,8
    ldir
    ld a,0xC3          ; JP opcode
    ld (0x0008),a
    ld hl,rst8_entry
    ld (0x0009),hl

    ;temp disable install of ISR for now
    ;ret

    ; Hook RST 38 (IM1)
    ld hl,0x0038
    ld de,orig_rst38_bytes
    ld bc,8
    di                 ; Disable interrupts while patching; JP opcode    
    ldir
    ld a,0xC3    
    ld (0x0038),a
    ld hl,im1_entry
    ld (0x0039),hl
    ei
    ret

; Restore original bytes
_rst8_restore:
    ; Restore RST 08
    ld hl,orig_rst8_bytes
    ld de,0x0008
    ld bc,8
    ldir
    ; Restore RST 38
    di
    ld hl,orig_rst38_bytes
    ld de,0x0038
    ld bc,8
    ldir
    ei
    ret

; After RST 08 hardware pushes return addr, we push AF BC DE HL IX IY
rst8_entry:
    di      ; good for stable debugging state 
    ; save current SP for C (before we push anything else beyond our fixed set)
    ld (_rst8_sp_copy),sp

    ; for safety we should switch to our own private stack here as we have no idea about the state of the stack
    ld sp, safe_stack_top    

    push af
    push bc
    push de
    push hl
    push ix
    push iy
    ld (_our_sp_base),sp    
    call _rst8_c_trap
    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    pop af
    ld sp, (_rst8_sp_copy)  ; return address will have been set in the C code
    ei
    ret

; IM1 RST 38 hook: minimal, calls C DART ISR shim then chains to original RST38
im1_entry:

    push af
    ; gate on enable flag - MAYBE NOT NEEDED AS INTERRUPTS ARE DISABLED DURING RST8 OPERATIONS - but any BDOS calls (logging might re-enable them ?! - should not make these!)
    ld a, (_enable_serial_interrupt)
    or a
    jr z, do_nothing

    push bc

    ; Read DART status register (RR0)
    in a,(DART_CTRL)
    ld b,a
    ; RX available?
    bit 0,b
    jr z, no_rx

    ; Read received byte (clears Rx IRQ)
    ld a,1
    ld (_wasInterrupt),a
    in a,(DART_DATA)
    ld c,a
    ; Ctrl-C?
    cp 0x03
    jr nz, no_ctrl_c

    ; Skip if stepping / restoring
    ld a,(gdbserver_trap_flags)
    and STEPPING_MASK
    jr nz, already_stepping

    push hl

    ; Set temp breakpoint at interrupt return address (PC at SP+8)
    push ix
    ld ix,0
    add ix,sp
    ; Load the word at SP+10 into HL. On a Z80 interrupt, the CPU pushes the return PC (low, high) at SP. 
    ; ISR has since pushed 5 registers (AF, BC, HL, IX = 8 bytes), the original PC is now at SP+8. Hence L gets low byte at SP+8 and H gets high byte at SP+9, so HL = interrupted PC    
    ld l,(ix+8)
    ld h,(ix+9)
    pop ix
    ld (temporary_breakpoint),hl

    ; Save original instruction and plant RST 08 (0xCF)
    ld a,(hl)
    ld (original_instruction),a
    ld a,0xCF
    ld (hl),a

    pop hl

    ; Flag: request post-trap step
    ld a,(gdbserver_trap_flags)
    or TRAP_FLAG_STEP_INSTRUCTION
    ld (gdbserver_trap_flags),a

 ;   jr no_rx


  ; send "$E10#??" via __z88dk_fastcall: dart_putc(uint8_t ch)
;    ld   de, gdb_busy
;    ld   b, 7             ; 7 bytes to send
;send:
;    ld   a,(de)
;    ld   l,a              ; fastcall: 8-bit arg in L
;    call _dart_putc
;    inc  de
;    djnz send

    ; else: drop non-^C bytes (do nothing)



no_ctrl_c:
already_stepping:
no_rx:
    pop bc
do_nothing:
    pop af

    ; CHAIN TO ORIGINAL HANDER
    jp orig_rst38_bytes


;gdb_busy: defb "$E10#A6"