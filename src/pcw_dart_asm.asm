

    PUBLIC INITCOMS
    PUBLIC SENDBYTE
    PUBLIC RECIEVEBYTE
	PUBLIC ENABLEINTS
	PUBLIC DISABLEINTS
	PUBLIC CLEAR_DTR

    HWFC EQU 1 ;ENABLE HARDWARE FLOW CONTROL

    ;PORTS
    STATUS		EQU		0E1h
    DATA		EQU		0E0h

    ;RR1
    RR1			EQU		1

    ;WR0
    WR0			EQU		0
    RESCHN	 	EQU     00011000B       ;Reset channel
    RESSTA	 	EQU     00010000B       ;Reset ext/status
    RESERR	 	EQU     00110000B       ;Error reset

    ;WR1
    WR1			EQU		1
    WRREG1_INT 	EQU     00001100B       ;WR1 - int on all rx chars
	WRREG1_OFF 	EQU     00000000B       ;WR1 - int on all rx chars


    ;WR3
    WR3			EQU		3
    WRREG3	 	EQU     11000001B       ;(C1) WR3 - Rx 8 bits/char, Rx enable
    ;WRREG3	 	EQU     11100001B       ;(C1) WR3 - Rx 8 bits/char, Rx enable - auto hw mode !!!!not sure need this ? 

    ;WR4
    WR4			EQU		4
    WRREG4	 	EQU     01000100B       ;WR4 - 16x, 1 stop bit, no parity

    ;WR5:
    WR5	    	EQU     5
    WR5DTROFF 	EQU     01101010B       ;(6A) DTR off, Tx 8 bits, Tx enable, RTS on
    WR5DTRON  	EQU     11101010B       ;(EA) DTR on, Tx 8 bits, Tx enable, RTS on
    WR5BREAK	EQU		00010000B		;SEND Break




;CORRUPTS NOTHING
;WAITCTS:
;	PUSH AF		; preserve AF
;_WAITSEND:

	;;check if we can write
;	IN A,(STATUS)		;for register 0 we can just read straight from port
;	BIT 2,A	   ;Wait for bit 2 to be 1 (transmit buffer empty) //for non hw handshaking
;	JP Z,_WAITSEND
;	POP AF
;	RET

;CORRUPTS NOTHING
FWAITCTS:
	PUSH AF		; preserve AF
_FWAITSEND:
IFDEF HWFC
	;;check for CTS 
	IN A,(STATUS)		;for register 0 we can just read straight from port
	BIT 5,A	   			;check for CTS
	JP Z,_FWAITSEND
_FCHECKSENT:
	LD A,RR1
	OUT (STATUS),A		;select register 1
	IN A,(STATUS)			;read register 1
	AND 1				;check if bit 0 is set - all sent ? 
	JP Z,_FCHECKSENT
ELSE
	;;check if we can write
	IN A,(STATUS)		;for register 0 we can just read straight from port
	BIT 2,A	   ;Wait for bit 2 to be 1 (transmit buffer empty) //for non hw handshaking
	JP Z,_FWAITSEND
ENDIF	
	POP AF
	RET


;CORRUPTS AF ;maybe inline this ? 
FWAITAVAIL:
	OR A		;Clear the carry flag
IFDEF HWFC
	DI		 ;ensure interrupts are disabled while we do this - should be quick	

ENDIF
_FWAITREC:
	;;check if byte available
	IN A,(STATUS)  ;for register 0 we can just read straight from port
	BIT 0,A     ; test data available (preserves C)

IFDEF HWFC
	JP Z,_FASTSETDTR

	;immediately clear DTR
CLEAR_DTR:
	LD A,WR5		;Access register 5
	OUT (STATUS),A
	LD A,WR5DTROFF
	OUT (STATUS),A

	RET
_FASTSETDTR:
	JP C,_FWAITREC	;if we already raised DTR no need to do it again

	SCF				; set carry flag to note we have raised DTR

	;Raise DTR - incase we havent already advertised we are waiting 
	LD A,WR5		;Access register 5
	OUT (STATUS),A
	LD A,WR5DTRON
	OUT (STATUS),A

	JP _FWAITREC
ELSE
	JP Z,_FWAITREC
	RET
ENDIF


;CORRUPTS NOTHING
;WAITAVAIL:
	;PUSH AF
;_WAITREC:

	;;check if byte available
;	IN A,(STATUS)  ;for register 0 we can just read straight from port
;	BIT 0,A	   ;Wait for bit 0 to be 1 (data available) 

;	ifdef HWFC
;	JR NZ,_READY
	
	;Raise DTR - incase we havent already advertised we are waiting 
;	LD A,WR5		;Access register 5
;	OUT (STATUS),A
;	LD A,WR5DTRON
;	OUT (STATUS),A

;	JR _WAITREC
;	else
;	JR Z,_WAITREC
;	endif
;_READY:
;	POP AF
;	RET

;CAREFUL -currently assumes B=255 when D>0 !!!!! - otherwise need to reset B each decrement of D
;Enter With HL = data start address; D=Number of outer loops; B=Number of inner loops
;CORRUPTS AF CD HL B
SENDDATA:
	LD C,DATA ;set data port
SENDNEXT:
	CALL FWAITCTS	;wait for OK to send byte ...
	OUTI
	JP NZ,SENDNEXT ;jump to next byte if B has not reached 0
	DEC D
	JP NZ,SENDNEXT ; outer loop
	RET

;CAREFUL -currently assumes B=255 when D>0 !!!!! - otherwise need to reset B each decrement of D
;Enter With HL = data start address; D=Number of outer loops; B=Number of inner loops
;CORRUPTS AF CD HL B
RECEIVEDATA:
	LD C,DATA ;set data port
_NEXTRECEIVEDATA:	
	;CALL WAITAVAIL 	;wait for OK to get byte ...
	CALL FWAITAVAIL 	;wait for OK to get byte ...

	; do this in fast wait instead
	;ifdef HWFC
	;Clear DTR
	;LD A,WR5		;Access register 5
	;OUT (STATUS),A
	;LD A,WR5DTROFF
	;OUT (STATUS),A
	;endif

	INI	

	JP NZ,RECEIVEDATA ;jump to next byte if B has not reached 0
	DEC D
	JP NZ,RECEIVEDATA ; outer loop
	RET


SENDBYTE:
	CALL FWAITCTS
	OUT (DATA),A
	RET

	;Returns byte in A
	;corrupts F
RECIEVEBYTE:
	CALL FWAITAVAIL
	IN A,(DATA)
	RET

INITCOMS:
	PUSH AF
	PUSH BC

	; SET BAUD RATE
	LD	A,36h
	OUT (0E7h), A		;
	;LD A,7h			;	Set TX 19200 baud 
	LD A,4h				;	Set TX 31250 baud 
	OUT (0E4h), A		
	XOR A	
	OUT (0E4h), A		;
	
	LD A,76h
	OUT (0E7h), A		;
	;LD A,7h			;	Set TX 19200 baud 
	LD A,4h
	OUT (0E5h), A		;	Set RX 31250 baud 
	XOR A
	OUT (0E5h), A		;

	
	;SET STATUS PORT
	LD C, STATUS

	;RESET DART
	LD A,RESCHN        
	OUT (C), A		
	
	LD A,WR4 				
	OUT (C),A
	LD A,WRREG4			;WR4 - 1 stop bit,no parity - x16 clock mode
	OUT (C),A

	LD A,WR1 			
	OUT (C),A
	LD A,WRREG1_OFF   	;WR1 - No interrupts		
	OUT (C),A			

	LD A,WR3 	
	OUT (C),A
	LD A,WRREG3			;WR3 - Rx 8 bit chars and Rx enable
	OUT (C),A

	LD A,WR5	
	OUT (C),A
IFDEF HWFC
	LD A,WR5DTROFF	;WR5 - Tx 8 bit chars, Tx enable. RTS on, DTR OFF
ELSE
	LD A,WR5DTRON		;WR5 - Tx 8 bit chars, Tx enable. RTS on, DTR ON
ENDIF
	OUT (C),A

    POP BC
	POP AF

	RET                     ;Return from call to this subroutine

ENABLEINTS:	;clobber AF - leaves ints disabled
	DI
	LD A,WR1
	OUT (STATUS),A
	LD A,WRREG1_INT
	OUT (STATUS),A

	; enable DTR so we can receive data
	LD A,WR5		;Access register 5
	OUT (STATUS),A
	LD A,WR5DTRON
	OUT (STATUS),A

	RET

DISABLEINTS: ;clobber AF - leaves ints disabled
	DI

	; disable DTR so we stop receiving data until ready
	; enable DTR so we can receive data
	LD A,WR5		;Access register 5
	OUT (STATUS),A
	LD A,WR5DTROFF
	OUT (STATUS),A

	LD A,WR1
	OUT (STATUS),A
	LD A,WRREG1_OFF	;turn off ints
	OUT (STATUS),A

	RET
