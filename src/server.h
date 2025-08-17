#ifndef __SERVER_H
#define __SERVER_H

#include <stdint.h>

extern uint8_t server_init(void);
extern uint8_t server_listen(void);
extern uint8_t server_read_data(void);
extern void server_write_packet(const char *data);

#endif