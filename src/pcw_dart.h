#ifndef PCW_DART_H
#define PCW_DART_H

#include <stdint.h>

void dart_init(void);
//uint8_t dart_rx_ready(void);
void wait_dart_rx_ready(void);
uint8_t dart_getc(void);
uint8_t dart_tx_ready(void);
//void wait_dart_tx_ready(void);
void dart_putc(uint8_t ch);

#endif
