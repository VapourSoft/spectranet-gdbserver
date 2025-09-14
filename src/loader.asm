; This file is based on code from the DASMed version of SIDs debugging part By WDT_ Cirsovius
; see:  https://mark-ogdenDT_uk/mirrors/wwwDT_cirsoviusDT_de/CPM/Projekte/Disassembler/ZSID/SIDCMD-MACDT_txt

; this is a custom BDOS handler located safely in the RSX area
; its job is to load a program for debugging and enter the debugger


PUBLIC RunENTRY

;emble RSX loading
RSX     equ 1

FALSE	equ	0
TRUE	equ	1

OS	equ	0000h
BDOS	equ	0005h
FCB	equ	005ch
CCPline	equ	0080h
DMA	equ	CCPline
RecPtr	equ	FCB-1
TPA	equ	0100h

BIOSvec	equ	32		; BIOS less cold start

RecLng	equ	128

OSErr	equ	255

FCBcpy	equ	16
LinLen	equ	64
IF	Z80
StkDep	equ	21
ELSE
StkDep	equ	19
ENDIF	; Z80

DT_nam	equ	 1
DT_ext	equ	 9
DT_fcb2	equ	16
DT_cr	equ	32

; Use z88dk / z80asm standard section names so data and code are placed
; into the expected sections when building with zcc/z80asm.
SECTION bss_user

AT_nam	equ	 8
AT_ext	equ	 3
FN	equ	AT_nam+AT_ext

DT_OS	equ	  0
DT_conin	equ	  1
DT_conout	equ	  2
SECTION code_user

DT_GetLin	equ	 10
DT_consta	equ	 11
DT_open	equ	 15
DT_close	equ	 16
DT_delete	equ	 19
DT_RdSeq	equ	 20
DT_WrSeq	equ	 21
DT_make	equ	 22
DT_setdma	equ	 26
DT_Conset	equ	109

_MaxBnk	equ	15
_SelMem	equ	27		; BIOS function
_TPA	equ	 1

;_RST	equ	11000111b	; Base RST code
;DT_RST	equ	6		; RST number
;RSTDT_adr	equ	DT_RST * 8	; Resulting RST memory address
;RSTDT_cod	equ	_RST + RSTDT_adr	; Resulting RST code

_Lines	equ	 12		; Number of list in L command
_Dump	equ	192		; Number of bytes to be dumped
_DByte	equ	 16		; Number of bytes per line
_Pass	equ	  8		; Max pass points
_PasLen	equ	  4		; Length of pass element
_SymLen	equ	 16		; Max length of symbol

RDT_Mask	equ	00111000b

_JP	equ	11000011b	; JP code
_CALL	equ	11001101b	; CALL code
_HALT	equ	01110110b	; HALT code
_INRDT_M	equ	00110100b	; INC (HL) code
_DCRDT_M	equ	00110101b	; DEC (HL) code
_MVIDT_M	equ	00110110b	; LD (HL),dd code
_JPDT_R	equ	11101001b	; JP (r) code
_LDDT_IM	equ	00100001b	; LD r,d16 code
_LDDT_ID	equ	00100010b	; LD (d16),r code
_LD8	equ	01110000b	; LD r,r code
_BIT	equ	11001011b	; BIT prefix
_DD	equ	11011101b	; Z80 prefix

nul	equ	00h
tab	equ	09h
lf	equ	0ah
cr	equ	0dh
eof	equ	1ah
del	equ	07fh

UPmask	equ	01011111b
LoMask	equ	00001111b
HiMask	equ	11110000b
NoMSB	equ	01111111b
MSB	equ	10000000b
PSWmask	equ	11111110b
LSB	equ	00000001b

_LAST	equ	0ffffh		; Last possible 16 bit address

_Untrc	equ	1
_Trace	equ	2

MSZE:
	dw	0
LAflag:
	db	FALSE
NEXT:
	dw	0    
TraceVal:
	dw	0    
LoadVal:
	dw	0    

SavHL:			;  -4 : HL	- 0000
	dw	0

; --------------------------------------------------
; UNRESOLVED SYMBOLS / STUBS
; The original code references many symbols supplied by
; other modules or DASM macros. Per your instruction these
; are declared here as minimal placeholders. Replace with
; proper definitions later.
; Code stubs: callable labels (return immediately)
BreakPoint:  ret  ; placeholder

; Data/value stubs (reserve 2 bytes each)
ENTRY:       dw 0  ; placeholder
SYMB:        dw 0  ; placeholder
BaseSym:     dw 0  ; placeholder
AT_SelBank:  dw 0  ; placeholders

; External labels provided by other modules

DLR_FCB:
	ds	FCBcpy

; End unresolved stubs

; Minimal C entry point for zcc linking
global _main


_main:
	jp RunENTRY


NoFile:
    ld hl, no_file
    call String

LoadSYM: ;not implemented

Main:
    ld hl, ready
    call String
    RST 08
    jmp 0x0100 ; ok  for com noit for RSX ?!


;
; ############################################################
; #                                                          #
; # Reduced entry of SID -- Less (Dis)Assembler commands L/A #
; #                 (Record boundary)                        #
; #                                                          #
; ############################################################
;
AT_BDOS:				; DT_DT_ new start
	jp	goBDOS
AT_RunENTRY:
	jp	RunENTRY
;
; *DT_UTL jump vectors
;
AT_BreakPoint:
	jp	BreakPoint	; + 0

IF	BANK
AT_LdBank:
	jp	LdBank
AT_StBank:
	jp	StBank
ENDIF	; BANK
;
; ##########################
; ## New BDOS entry point ##
; ##########################
;
goBDOS:
    jmp BDOS ; 


RunENTRY:
    ; print message
    ld hl,message
    call String
    
	ld	a,(FCB+DT_nam)	; Test file name here
	cp	' '
	jp	z,NoFile		; DT_DT_ no, start
	ld	a,(FCB+DT_ext)
	cp	' '		; Test primary extension
	jp	nz,ExtOk
	call	SetDT_COM
	ld	a,(FCB+DT_fcb2)
	cp	' '		; Test secondary FCB
	jp	z,ExtOk
	ld	a,(FCB+DT_fcb2+DT_ext)
	cp	' '		; Test secondary extension
	jp	nz,ExtOk
	call	SetDT_SYM
ExtOk:
	ld	hl,0
;
; Load file(s)
; ENTRY	Reg HL holds entry address
;
LoadFile:
	ld	(LoadVal),hl	; Save entry

;   might need to do this as well as ensure bank if this isnt being executed from a program in the TPA ?!
;	ld	de,DMA
	call	SetDMA		; Set default DMA

	ld	hl,FCB+DT_fcb2
	ld	de,DLR_FCB
	ld	c,FCBcpy
Sav2ndFCB:
	ld	a,(hl)
	ld	(de),a		; Save 2nd file name
	inc	hl
	inc	de
	dec	c
	jp	nz,Sav2ndFCB
	ld	a,(FCB+DT_nam)	; Test wild card
	cp	'?'
	jp	z,LoadSYM	; .. only symbol table
	call	Open		; Find ,COM file
	cp	OSErr
	jp	z,ERROR
	ld	hl,TPA
	ld	(NEXT),hl	; Init address
	ld	hl,(LoadVal)
	ld	de,TPA
	add	hl,de
LoadCOMloop:
	push	hl
	ld	de,FCB
	ld	c,DT_RdSeq
	call	goBDOS		; Get record
	pop	hl
	or	a		; DT_DT_ test more
IF	RSX
	jp	nz,TryRSX
ELSE
	jp	nz,LoadSYM
ENDIF	; RSX
	ld	de,DMA
	ld	c,RecLng
UnpkCode:
	ld	a,(de)
	inc	de
	ld	(hl),a		; DT_DT_ unpack code
	inc	hl
	dec	c
	jp	nz,UnpkCode
	call	SetMSZE		; Set addresses
	call	SetNEXT
	ex	de,hl
	ld	hl,(BDOS+1)
	call	CmpDT_HLDT_DE	; Test still room
	ex	de,hl
	jp	nc,LoadCOMloop
	ld	hl,TPA
	ld	(NEXT),hl	; Clear addresses
	ld	(MSZE),hl
	jp	ERROR		; DT_DT_ and tell error

IF	RSX

BytMask	equ	00000111b
SCBget	equ	49
HdLen	equ	16
SCBini	equ	0003h
RSXhd	equ	0100h
MinPag	equ	15
_RET	equ	0c9h
_COMM	equ	05dh
_LOAD	equ	018h
_PREV	equ	00dh
_PRDT_LO	equ	00bh

;
; DT_COM file loaded, try RSX
;
TryRSX:
	ld	de,SCBadr
	ld	c,SCBget
	call	BDOS		; Get COMMON base
	ld	(SavSCB),hl
	ld	hl,(NEXT)	; Get load top
	ld	a,l
	and	MSB		; Test page boundary
	jp	z,RSXpage	; DT_DT_ yeap
	inc	h		; DT_DT_ fix top
	ld	l,0
RSXpage:
	ld	(TopOvl),hl	; DT_DT_ set top
	ld	hl,TPA
	ld	a,(hl)		; Test RSX
	cp	_RET
	jp	nz,LoadSYM	; DT_DT_ nope
;
; The RSX load task
; Reg HL points to current RSX slot
;
LoadGENCOM:
	ld	de,HdLen
	add	hl,de		; Point to RSX address
	push	hl
	ld	e,(hl)		; DT_DT_ get it
	inc	hl
	ld	d,(hl)
	ld	a,e
	or	d		; Test end
	jp	nz,LoadRSX
	call	TPA+SCBini	; Init SCB
	ld	hl,(TPA+1)
	ld	b,h		; Get length
	ld	c,l
	ld	de,TPA
	ld	hl,TPA+RSXhd
	ld	a,(hl)		; Test code
	cp	_RET
	jp	z,LoadSYM
	ld	a,b
	or	c		; DT_DT_ or any length
	call	nz,ldir		; DT_DT_ unpack
	jp	LoadSYM
LoadRSX:
	inc	hl
	ld	c,(hl)		; Fetch length
	inc	hl
	ld	b,(hl)
	ld	a,(SavSCB)	; Test COMMON
	or	a
	jp	z,ldRSX1	; DT_DT_ unbanked
	inc	hl
	inc	(hl)		; Test banked
	jp	z,ldRSX2
ldRSX1:
	push	de		; Save load address
	call	TopLess		; Set up addresses
	pop	hl
	call	PageReloc	; Relocate
	call	InitRSX		; Fix RSX environment
ldRSX2:
	pop	hl
	jp	LoadGENCOM	; DT_DT_ get next RSX
;
; Check RSX load address in range
; ENTRY	Reg BC holds length of code
; EXIT	Reg DE holds load address
;
TopLess:
	ld	a,(BDOS+2)	; Get page
	dec	a
	dec	bc
	sub	b		; Test room
	inc	bc
	cp	MinPag
	jp	c,ERROR		; DT_DT_ nope
	ld	hl,(TopOvl)
	cp	h
	jp	c,ERROR
	ld	d,a		; DT_DT_ set start
	ld	e,0
	ret
;
; Page relocation
; ENTRY	Reg BC holds length of program
;	Reg DE holds load address
;	Reg HL points to PRL type header
;
PageReloc:
	inc	h		; Fix page
	push	bc
	push	de
	call	ldir		; DT_DT_ unpack program code
	pop	de
	pop	bc
	push	de		; Save load address
	ld	e,d		; Get page
	dec	e		; DT_DT_ fix
	push	hl		; Save start of bit map
	ld	h,e		; Get relocation page
	ld	e,0		; Clear bit count
PRLbit:
	ld	a,b		; Test ready
	or	c
	jp	z,donePRL
	dec	bc
	ld	a,e
	and	BytMask		; Test byte boundary
	jp	nz,PRLbit1	; DT_DT_ nope
	ex	(sp),hl
	ld	a,(hl)		; Get map byte
	inc	hl
	ex	(sp),hl
	ld	l,a
PRLbit1:
	ld	a,l
	rla			; Test bit
	ld	l,a
	jp	nc,PRLbit2	; DT_DT_ nope
	ld	a,(de)
	add	a,h		; Relocate
	ld	(de),a
PRLbit2:
	inc	de
	jp	PRLbit
donePRL:
	pop	de		; DT_DT_ clean stack
	pop	de
	ret
;
; Init RSX header
; ENTRY	Reg DE holds RSX start address
;
InitRSX:
	ld	hl,(BDOS+1)	; Get current address
	ld	l,0		; DT_DT_ boundary
	ld	bc,6
	call	ldir		; Copy copyright
	ld	e,_LOAD
	xor	a
	ld	(de),a		; Clear LOADER
	ld	e,_PREV
	ld	(de),a		; Set previous to 0007h
	dec	de
	ld	a,BDOS+2
	ld	(de),a
	ld	l,e		; DT_DT_ copy current
	ld	e,_PRDT_LO
	ld	(hl),e		; Set NEXT
	inc	hl
	ld	(hl),d
	ex	de,hl
	ld	(hl),d		; Set offset next HI
	dec	hl
	ld	(hl),BDOS+1
	ld	l,BDOS+1
	ld	(BDOS+1),hl	; DT_DT_ new vector
	ret
ldir:
	ld	a,b
	or	c
	ret	z
	dec	bc
	ld	a,(hl)
	ld	(de),a
	inc	hl
	inc	de
	jp	ldir
SCBadr:
	db	_COMM,0
SavSCB:
	dw	0
TopOvl:
	dw	0
ENDIF	; RSX

;
; Set extension .COM into primary FCB
;
SetDT_COM:
	ld	hl,FCB+DT_ext	; Set extension pointer
	ld	(hl),'C'	; .. set .COM
	inc	hl
	ld	(hl),'O'
	inc	hl
	ld	(hl),'M'
	ret
;
; Set extension .SYM into secondary FCB
;
SetDT_SYM:
	ld	hl,FCB+DT_fcb2+DT_ext
	ld	(hl),'S'	; .. set .SYM
	inc	hl
	ld	(hl),'Y'
	inc	hl
	ld	(hl),'M'
	ret

;
; Open file
; EXIT	Accu holds I/O code
;
Open:
	push	hl
	push	de
	push	bc
	xor	a
	ld	(RecPtr),a	; Clear pointer
	ld	c,DT_open
	ld	de,FCB
	call	goBDOS		; .. open file
	pop	bc
	pop	de
	pop	hl
	ret
;
; Close file
; ENTRY	Reg DE holds FCB
; EXIT	Accu holds I/O code
;
Close:
	push	bc
	push	de
	push	hl
	ld	c,DT_close
	call	goBDOS		; .. close
	pop	hl
	pop	de
	pop	bc
	ret

;
; Set MSZE address
; ENTRY	Reg HL holds current address
;
SetMSZE:
	call	ChkAddr		; Test address
	ret	nc		; .. too high
	ld	(MSZE),hl	; .. else set
	ret
;
; Set NEXT address
; ENTRY	Reg HL holds current address
;
SetNEXT:
	ex	de,hl
	ld	hl,(NEXT)	; Get NEXT
	ld	a,l
	sub	e		; .. compare
	ld	a,h
	sbc	a,d
	ex	de,hl
	ret	nc		; .. too high
	ld	(NEXT),hl	; .. else set
	ret

ERROR:
	call	CrLf
    ld hl, err_msg
    call String
	ret             ; return to loader program ?!

;
; Compare regs DE and HL
; EXIT	Zero flag set if equal
;	Carry flag set if HL < DE
;
CmpDT_HLDT_DE:
	ld	a,h
	cp	d		; .. compare
	ret	c
	ret	nz
	ld	a,l
	cp	e
	ret	c
	ret	nz
	xor	a
	ret
    
;
; Load chararacter from file
; EXIT	Accu holds character
;
Get:
	push	hl
	push	de
	push	bc
	ld	a,(RecPtr)	; Get current pointer
	and	RecLng-1	; .. test any left
	jp	z,GetDT_file
GetDT_buf:
	ld	d,0
	ld	e,a
	ld	hl,DMA
	add	hl,de		; .. get buffer address
	ld	a,(hl)
	cp	eof		; Test EOF
	jp	z,GetDT_ex
	ld	hl,RecPtr
	inc	(hl)		; Bump pointer
	or	a
	jp	GetDT_ex
GetDT_file:
	ld	c,DT_RdSeq
	ld	de,FCB
	call	goBDOS		; Read record
	or	a
	jp	nz,GetDT_EOF	; .. test more
	ld	(RecPtr),a	; Clear pointer
	jp	GetDT_buf
GetDT_EOF:
	ld	a,eof		; .. return EOF
GetDT_ex:
	pop	bc
	pop	de
	pop	hl
	ret

;
; Compare address againts pointer
; ENTRY	Reg HL points to address
; EXIT	Carry set indicates OK
;
ChkAddr:
	ex	de,hl
	ld	hl,(MSZE)	; Get pointer
	ld	a,l
	sub	e		; .. compare
	ld	a,h
	sbc	a,d
	ex	de,hl
	ret

;
; Remove List and Assemble part of SID
;
RemoveLA:
	ld	a,TRUE
	ld	(LAflag),a	; Indicate no L and A
	ret    

;
; Compare three bytes against FCB extension
; ENTRY	Accu  holds 1st byte
;	Reg B holds 2nd byte
;	Reg C holds 3rd byte
; EXIT	Zero flag set if found
;
Compare:
	ld	hl,FCB+DT_ext-1	; Init FCB pointer
	call	CmpFCBchr	; Check 1st
	ret	nz		; .. no
	ld	a,b
	jp	CmpDT_2DT_FCBchr	; Check last ones    

;
; Print string on console
; ENTRY	Reg HL points to zero closed string
;
String:
	ld	a,(hl)		; Get character
	or	a
	ret	z		; .. test end
	call	Conout		; .. print
	inc	hl
	jp	String

;
; Close console line
;
CrLf:
	ld	a,cr
	call	Conout		; .. give CR
	ld	a,lf
	jp	Conout		; .. and LF

;
; Print character on console
; ENTRY	Accu holds character
;
Conout:
	push	hl
	push	de
	push	bc
	ld	e,a
	ld	c,DT_conout
	call	goBDOS		; .. print
	pop	bc
	pop	de
	pop	hl
	ret    


;
; Compare FCB position against characters
; ENTRY	Accu  holds 1st character
;	Reg C holds 2nd character
;	Reg HL holds pointer
; EXIT	Zero set if same
;
CmpDT_2DT_FCBchr:
	call	CmpFCBchr	; Test 1st
	ret	nz
	ld	a,c
	call	CmpFCBchr	; Test 2nd on success
	ret

; Compare FCB position against character
; ENTRY	Accu holds character
;	Reg HL holds pointer
; EXIT	Zero set if same
;
CmpFCBchr:
	inc	hl
	cp	(hl)		; Compare
	ret	z		; .. ok
	or	MSB
	cp	(hl)		; Compare SYS bit
	ret    

; set DMA to top-of-TPA minus RecLng (128)
SetDMA_TOP:
    di                        ; optional: avoid races while changing DMA
    ld   a,(BDOS+2)           ; page count from BDOS
    dec  a                    ; use (BDOS+2)-1 as top page
    ld   h,a
    ld   l,0                  ; HL = top_page * 256
    ld   c,RecLng             ; number of bytes to back off (128)
set_dma_loop:
    dec  hl
    dec  c
    jr   nz,set_dma_loop     ; HL now = top - RecLng
SetDMA:
    ld   de,hl                ; DMA address in DE
    ld   c,DT_setdma
    call goBDOS               ; BDOS sets DMA = DE
    ei

message:    db "Loader executing...",cr,lf,0
ready:      db "Loader finished",cr,lf,0
no_file:    db "No File Specified",cr,lf,0
err_msg:    db "Error unable to load file",cr,lf,0