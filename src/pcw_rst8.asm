; Minimal RST 08 hook for Amstrad PCW (CP/M)
; Captures registers and calls C helper; no Spectranet dependencies.

    PUBLIC _rst8_install
    PUBLIC _rst8_restore
    PUBLIC rst8_entry
    PUBLIC im1_entry
    PUBLIC orig_rst38_bytes
    PUBLIC _interrupt_depth

    EXTERN _rst8_c_trap
    EXTERN _rst8_sp_copy
    EXTERN _our_sp_base
    EXTERN _enable_serial_interrupt ; flag to enable/disable serial interrupt checking in C code

    EXTERN ENABLEINTS
    EXTERN DISABLEINTS
    EXTERN CLEAR_DTR

    DART_DATA  equ  0xE0
    DART_CTRL  equ  0xE1

    EXTERN _gdbserver_state
    EXTERN _wasInterrupt

    defc gdbserver_trap_flags = _gdbserver_state
    defc temporary_breakpoint = gdbserver_trap_flags + 1 
    defc original_instruction = temporary_breakpoint + 2

    defc TRAP_FLAG_STEP_INSTRUCTION =  0x02


    ; Use z88dk standard section names so code isn't lost / zeroed
    SECTION bss_user

_interrupt_depth: 
    defs 1              ; depth of nested interrupts (0 = not in ISR, >0 = in ISR)

ints_enabled_flag:
    defs 1              ; set to 1 if interrupts were enabled on entry to RST8, else 0

safe_stack: 
    defs 256         ; 256 bytes for private stack
safe_stack_top:              ; label for top of stack (highest address)

orig_rst8_bytes:
    defs 8              ; storage for original 8 bytes at 0008h

orig_rst38_bytes:
    defs 8              ; storage for original 8 bytes at 0038h (IM1)

orig_rst8_bytes_bank_0:
    defs 8              ; storage for original 8 bytes at 0008h for bank 0

orig_rst38_bytes_bank_0:
    defs 8              ; storage for original 8 bytes at 0008h for bank 0

_our_sp_base:
    defs 2              ; storage for our private stack base (so C can get at our pushed regs ) )

_rst8_sp_copy:
    defs 2              ; space for saved SP

    SECTION code_user

; Install: copy original bytes then patch with JP rst8_entry
_rst8_install:

    di

    ; do page 0 first
    ld a, 0x80    
    out (0xF0),a       ; switch to page 0

    ; Hook RST 08
    ld ix,0x0008    
    ld hl,rst8_entry
    ld de,orig_rst8_bytes_bank_0           
    call _rst_install

    ; Hook RST 38 (IM1)
    ld ix,0x0038
    ld hl,im1_entry
    ld de,orig_rst38_bytes_bank_0           
    call _rst_install    

    ; go back and do TPA 4
    ld a, 0x84
    out (0xF0),a       ; switch to page        

    ; Hook RST 08
    ld ix,0x0008    
    ld hl,rst8_entry
    ld de,orig_rst8_bytes    
    call _rst_install

    ; Hook RST 38 (IM1)
    ld ix,0x0038
    ld hl,im1_entry
    ld de,orig_rst38_bytes
    call _rst_install    

    xor a    
    ld (_interrupt_depth),a ; reset interrupt depth counter

    ei
    ret

    ; copy 8 bytes from ix to de, patch with JP hl
    ; ix = RST location address to patch    - preserved
    ; de = dest of 8 original bytes to copy - clobbered
    ; hl = address of my handler - preserved
    ; bc clobbered
_rst_install:
    ;copy 8 bytes from hl to de
    push hl           ; save new handler address
    push ix
    pop  hl           ; HL = original handler address

    ld   bc,8
    ldir              ; copy 8 bytes from (HL) to (DE)

    pop hl           ; restore handler

    ld   a,0xC3       ; JP opcode
    ld   (ix+0),a
    ld   (ix+1),l       ; low byte of handler
    ld   (ix+2),h       ; high byte of handler

    ret

; Restore original bytes
_rst8_restore:

    ; Restore RST 08
    di

    ;bank 0
    ld a, 0x80    
    out (0xF0),a       ; switch to page 0

    ld hl,orig_rst8_bytes_bank_0
    ld de,0x0008
    ld bc,8
    ldir

    ;bank 4
    ld a, 0x84    
    out (0xF0),a       ; switch to page 4

    ld hl,orig_rst8_bytes
    ld de,0x0008
    ld bc,8
    ldir

    ; Restore RST 38
    ld hl,orig_rst38_bytes
    ld de,0x0038
    ld bc,8
    ldir
    ei
    ret

handleInterruptState:
    push af

    ; determine if interrupts are enabled or not so we can restore them later
    ld a,i
    di      ; disable them now we have captured the state 
    jp PO, disabled ; odd parity = interrupts def not enabled

    ;Executing LD A,I inside an ISR will place IFF2 (the pre‑interrupt enable state) into P/V, 
    ;not the current IFF1 state. That means LD A,I can show “interrupts were enabled before this ISR” even though IFF1 is presently 0 (disabled) inside the ISR
    ;so we need to see if we are in an ISR (we hooked this on setup) and if we are, we need to assume interrupts are currently disabled (they should be)
    ; if _interrupt_depth is > 0 then interrupts are disabled and a should bet set to 1
    ld a,(_interrupt_depth)
    or a    
    jp nz,disabled

    inc a   ; set a to 1
    jr enabled

disabled:
    xor a   ; set a to 0

enabled:
    ld (ints_enabled_flag),a  
    call DISABLEINTS        ;clobbers af - disable ints 
    pop af
    ret


; After RST 08 hardware pushes return addr, we push AF BC DE HL IX IY
rst8_entry:

    call handleInterruptState ;leaves ints disabled

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

    call ENABLEINTS ; clobbers af - enables SIO ints but leaves ints DISABLED!

    ld a,(ints_enabled_flag)
    or a
    jp z, restore_stack_and_ret

    ei


restore_stack_and_ret:
    pop af
    ld sp, (_rst8_sp_copy)  ; return address will have been set in the C code
    ret

; IM1 RST 38 hook: minimal, calls C DART ISR shim then chains to original RST38
im1_entry:

    push af

    ; interrupt_depth++
    ld   a,(_interrupt_depth)
    inc  a
    ld   (_interrupt_depth),a

    ; gate on enable flag
    ld a, (_enable_serial_interrupt)
    or a
    jr z, do_nothing

    ; Read DART status register (RR0)
    in a,(DART_CTRL)
    ; RX available?
    bit 0,a
    jr z, do_nothing

    ; Read received byte (clears Rx IRQ)
    in a,(DART_DATA)
    cp 0x03 ; Ctrl-C?
    jr nz, do_nothing

    ;turn off DTR to prevent remaining chars to be lost
    call DISABLEINTS

    ;ignore further serial interrupts until we re-enable them in C code
    xor a
    ld (_enable_serial_interrupt),a

    push hl

    ; Set temp breakpoint at interrupt return address (PC at SP+8)
    push ix
    ld ix,0
    add ix,sp
    ; Load the word at SP+10 into HL. On a Z80 interrupt, the CPU pushes the return PC (low, high) at SP. 
    ; ISR has since pushed 5 registers (AF, HL, IX = 6 bytes), the original PC is now at SP+6. Hence L gets low byte at SP+6 and H gets high byte at SP+7, so HL = interrupted PC    
    ld l,(ix+6)
    ld h,(ix+7)
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

do_nothing:
    pop af

 ; Place isr_epilogue on stack as next return target, preserving HL
    exx                  ; switch to alternate BC',DE',HL'
    ld   hl, isr_epilogue
    push hl              ; stack: [isr_epilogue][RET_PC]
    exx                  ; restore primary BC,DE,HL

    ; CHAIN TO ORIGINAL HANDER
    jp orig_rst38_bytes

isr_epilogue:
    push AF                 ; save A and F as set by the original ISR

    ; interrupt_depth--
    ld   a,(_interrupt_depth)
    dec  a
    ld   (_interrupt_depth),a

    pop  AF                 ; restore A and F
    ret        

;gdb_busy: defb "$E10#A6"