#ifndef PCW_RST8_H
#define PCW_RST8_H

#include <stdint.h>

void rst8_install(void);
void rst8_restore(void);
extern uint16_t rst8_sp_copy;
void rst8_c_trap(void);
extern unsigned char rst8_called; // 0 until rst8_install runs

#endif
