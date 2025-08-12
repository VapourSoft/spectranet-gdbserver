; Minimal RST 08 hook for Amstrad PCW (CP/M)
; Captures registers and calls C helper; no Spectranet dependencies.

    PUBLIC _rst8_install
    PUBLIC _rst8_restore
    PUBLIC rst8_entry
    PUBLIC _rst8_called

    EXTERN _rst8_c_trap
    EXTERN _rst8_sp_copy
    EXTERN _dart_putc          ; serial output helper (C symbol dart_putc)

    ; Use z88dk standard section names so code isn't lost / zeroed
    SECTION bss_user
orig_rst8_bytes:
    defs 8              ; storage for original 8 bytes at 0008h
_rst8_sp_copy:
    defs 2              ; space for saved SP
_rst8_called:
    defs 1              ; flag: 0 before install, set to 1 by _rst8_install

    SECTION code_user

; Install: copy original bytes then patch with JP rst8_entry
_rst8_install:
    ld hl,0x0008
    ld de,orig_rst8_bytes
    ld bc,8
    ldir
    ld a,0xC3          ; JP opcode
    ld (0x0008),a
    ld hl,rst8_entry
    ld (0x0009),hl
    ld a,1
    ld (_rst8_called),a ; mark called
    ; Debug side-effect: output 'I' over serial (dart_putc) to prove we executed.
    push af
    push bc
    ld a,'I'
    ld c,a               ; pass in L? depends on calling conv; easiest: push then call
    push af               ; push char parameter (sdcc_iy convention passes first via stack)
    call _dart_putc
    pop af
    pop bc
    pop af
    ret

; Restore original bytes
_rst8_restore:
    ld hl,orig_rst8_bytes
    ld de,0x0008
    ld bc,8
    ldir
    ret

; After RST 08 hardware pushes return addr, we push AF BC DE HL IX IY
rst8_entry:
    di
    ; save current SP for C (before we push anything else beyond our fixed set)
    ld (_rst8_sp_copy),sp
    push af
    push bc
    push de
    push hl
    push ix
    push iy
    call _rst8_c_trap
    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    pop af
    ei
    ret
