// PCW DART minimal polled backend (SDCC/ASxxxx compatible)
#include <stdint.h>

//Cant run in DEBUG when an RSX as it would need to pull in all the extra libs etc
#ifdef DEBUG
#include <stdio.h>
#endif

#define WR5_DTR_ON  0XEA 
#define WR5_DTR_OFF 0X6A


// PCW Z80 DART ports (status/command at 0xE1, data at 0xE0)
#define DART_DATA   0xE0
#define DART_CTRL   0xE1

// Status bits for SIO/DART RR0 (common):
// bit 0: Rx Character Available, bit 2: Tx Buffer Empty
#define RR0_RX_AVAIL 0x01
#define RR0_TX_EMPTY 0x04

#define DI __asm di __endasm
#define EI __asm ei __endasm

// Additional PCW-specific ports for baud generator
__sfr __at (0xE4) DART_BRG_TX_LO;
__sfr __at (0xE5) DART_BRG_RX_LO;
__sfr __at (0xE7) DART_BRG_IDX;

// SFRs for main DART ports
__sfr __at (0xE0) DART_DATA_PORT;
__sfr __at (0xE1) DART_CTRL_PORT;

// Forward static prototypes
static uint8_t inb(uint8_t port);
static void outb(uint8_t port, uint8_t val);

volatile uint8_t wasInterrupt = 0;

static uint8_t inb(uint8_t port) {
	if (port == DART_DATA) return DART_DATA_PORT;
	else return DART_CTRL_PORT;
}

static void outb(uint8_t port, uint8_t val) {
	if (port == DART_DATA) DART_DATA_PORT = val;
	else DART_CTRL_PORT = val;
}

void dart_init(void) {
	// Configure baud generator for 31250 as per original asm
	DART_BRG_IDX = 0x36;      // select TX param
	DART_BRG_TX_LO = 0x04;    // TX divisor
	DART_BRG_TX_LO = 0x00;    // commit

	DART_BRG_IDX = 0x76;      // select RX param
	DART_BRG_RX_LO = 0x04;    // RX divisor
	DART_BRG_RX_LO = 0x00;    // commit

	// Reset channel
	outb(DART_CTRL, 0x18);    // WR0: Reset Channel

	// WR4 - 16x clock, 1 stop bit, no parity
	outb(DART_CTRL, 4);
	outb(DART_CTRL, 0x44);

	// WR1 - No interrupts
	//outb(DART_CTRL, 1);
	//outb(DART_CTRL, 0x00);

 	// WR1 - Enable Rx interrupts (all characters), no Tx/ext status IRQs
    outb(DART_CTRL, 1);
    outb(DART_CTRL, 0x18);	

	// WR3 - Rx 8 bits/char, Rx enable
	outb(DART_CTRL, 3);
	outb(DART_CTRL, 0xC1);

	// WR5 - DTR on, Tx 8 bits, Tx enable, RTS on
	outb(DART_CTRL, 5);
	outb(DART_CTRL, WR5_DTR_OFF); 

	/*
	// SEND BREAK to try to reset and connect MODEM
	// Set WR5 register pointer
	outb(DART_CTRL, 5);
	outb(DART_CTRL, 0x2A | 0x80); // wr5_dtr_off = 0x2A; wr5_break = 0x80;

	delay (1000); // wait for a bit to ensure break is sent

	//STOP BREAK
	outb(DART_CTRL, 5);
	outb(DART_CTRL, 0x6A); // wr5_dtr_on = 0x6A; wr5_break = 0x00;
	*/

}



inline void wait_dart_rx_ready(void) {
	// Wait for a received character, handling DTR as in the original asm.
	// Returns 1 when data is available.
	static uint8_t dtr_raised = 0;
	
//	DI; 						// interrupts should be disabled in GDB loop anyway		

	while (1) {
		uint8_t status = inb(DART_CTRL);
		if (status & RR0_RX_AVAIL) {
			// Data available, clear DTR if we raised it
			if (dtr_raised) {
				outb(DART_CTRL, 5);        // Select WR5
				outb(DART_CTRL, WR5_DTR_OFF);     // WR5 DTR off (0x2A)
				dtr_raised = 0;
			}
			//enable interrupts
//			EI;							interrupts should be disabled in GDB loop anyway
			return;
		} else {
			// No data, raise DTR if not already done
			if (!dtr_raised) {
				outb(DART_CTRL, 5);        // Select WR5
				outb(DART_CTRL, WR5_DTR_ON);     // WR5 DTR on (0x6A)
				dtr_raised = 1;
			}
			// Loop again
		}
	}
}

extern void printS(const char* str) __z88dk_fastcall ;

uint8_t dart_getc(void) {
	if (wasInterrupt) 
	{
		printS("\n\rINTERRUPT DURING READ\r\n$");
		wasInterrupt = 0;
	}
	wait_dart_rx_ready();	
	return inb(DART_DATA);
}

inline void wait_dart_tx_ready(void) {
	
	// Simple busy-wait loop for a short delay (PCW compatible)
	//for (volatile uint16_t i = 0; i < 100; ++i) {
		// do nothing, just waste cycles
	//}

	// Wait for CTS (Clear To Send) signal and for all data to be sent
	// This function waits until CTS is asserted and the transmit buffer is empty

	// Wait for CTS (bit 5 of RR0)
	while (1) {
		uint8_t status = inb(DART_CTRL);
		if (status & (1 << 5)) { // CTS asserted
			break;
		}
		// else, keep waiting
	}

	// Wait for all data sent (bit 0 of RR1)
	while (1) {
		// Select RR1 (register 1)
		outb(DART_CTRL, 1);
		uint8_t rr1 = inb(DART_CTRL);
		if (rr1 & 0x01) { // All sent (bit 0 set)
			break;
		}
		// else, keep waiting
	}
}

void dart_putc(uint8_t ch) {

#ifdef DEBUG
  putchar((unsigned char)ch);
#endif

  wait_dart_tx_ready();

  outb(DART_DATA, ch);
}
