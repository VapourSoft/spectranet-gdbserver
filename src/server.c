// Serial-backed GDB server (PCW DART)

#include "server.h"
#include "utils.h"
#include "state.h"
#include "pcw_dart.h"
#include "pcw_rst8.h"

#ifdef TARGET_PCW_DART
#endif


#include <stddef.h>

extern void printS(const char* str) __z88dk_fastcall ;

extern void *__memcpy(void *dest, const void *src, size_t n) ;
//extern void *__memset(void *s, int c, size_t n);

//Called on startup to init state
uint8_t server_init(void)
{
    //Clear state
    //__memset(&gdbserver_state, 0, sizeof(gdbserver_state));

    return 0;
}

static void write_data_raw(const uint8_t *data, uint16_t len)
{
    // Send bytes over serial
    for (uint16_t i = 0; i < len; ++i)
        dart_putc(data[i]);
}

static void write_str_raw(const char *data)
{
    while (*data)
        dart_putc((uint8_t)*data++);
}

static  size_t strlen(const char *str)
{
    const char *s = str;
    while (*s)
        ++s;
    return s - str;
}

static char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; ++haystack)
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n))
        {
            ++h;
            ++n;
        }
        if (!*n)
            return (char *)haystack;
    }
    return NULL;
}

static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static char *__strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;
        ++s;
    }
    return (c == 0) ? (char *)s : NULL;
}


void write_packet_bytes(const uint8_t *data, uint8_t num_bytes)
{
    size_t i;

    //It might be too late by now but sanity check size anyway
    if (num_bytes + 4 > sizeof(gdbserver_state.w_buffer))
    {
        write_error();
        return;
    }

    char* wbuf = gdbserver_state.w_buffer;

    *wbuf++ = '$';
    __memcpy(wbuf, data, num_bytes);
    wbuf += num_bytes;
    *wbuf++ = '#';

    uint8_t checksum;
    for (i = 0, checksum = 0; i < num_bytes; ++i)
        checksum += data[i];
    to_hex(&checksum, wbuf, 1);

    write_data_raw(gdbserver_state.w_buffer, num_bytes + 4);
}

void server_write_packet(const char *data)
{
    write_packet_bytes((const uint8_t *)data, strlen(data));
}

uint8_t server_listen()
{
    // Serial has no concept of listen/accept; treat as always connected
    return 0;
}


static const struct {
    const char* request;
    const char* response;
} queries[] = {
    //{"Supported",               "PacketSize=128;NonBreakable;qXfer:features:read+;qXfer:auxv:read+"},
    {"Supported",               "PacketSize=128;qXfer:features:read+;qXfer:auxv:read+"},
    {NULL,                      NULL},
};

static void write_error()
{
    // print current input buffer to screen
    printS("\n\rERR > buffer [$");

    size_t buf_len = sizeof gdbserver_state.buffer;
    char *hash = __strchr((const char*)gdbserver_state.buffer, '#');

    char *term;
    if (hash == NULL || hash > (char*)gdbserver_state.buffer + buf_len - 3) {
        /* no '#' or checksum not fully present — ensure we place a '$' inside buffer */
        term = (char*)gdbserver_state.buffer + buf_len - 1;
    } else {
        /* place '$' after hash */
        term = (hash+3);
    }

    *term = '$'; /* CP/M BDOS print-string terminator */

    printS((const char*)gdbserver_state.buffer);
    printS("]\n\r$");

    server_write_packet("E01");
}

static void write_ok()
{
    server_write_packet("OK");
}

static uint8_t process_packet()
{
    char* payload = (char*)gdbserver_state.buffer;

    char command = *payload++;
    switch (command)
    {
        case 'c':
        {
            // continue execution
            return 0;
        }
        case 'k':
        {
            // DeZog send this to Mame on connect
            // kill the program
            //for now just ignore for compatibility
            write_ok();
            break;
        }
        case 's':
        {
            // do one step
            gdbserver_state.trap_flags |= TRAP_FLAG_STEP_INSTRUCTION;
            return 0;
        }
        case 'i': //this is non-standard but sent by z88dk-gdb when it wants to skip a call
        {
            // step over calls
            uint8_t offset = *payload - '0'; // simple atoi
            gdbserver_state.temporary_breakpoint.address = gdbserver_state.registers[REGISTERS_PC] + offset;
            gdbserver_state.trap_flags |= (TRAP_FLAG_STEP_INSTRUCTION | TRAP_FLAG_FORCE_ADDRESS); //Force this address (dont calculate) 
            return 0;
        }
        case 'q':
        {
            if (strstr(payload, "Xfer:features:read") == payload)
            {
                write_str_raw(
                    "$l<target version=\"1.0\"><feature name=\"org.gnu.gdb.z80.cpu\">"
                    "<reg name=\"sp\" bitsize=\"16\" type=\"data_ptr\"/>"
                    "<reg name=\"pc\" bitsize=\"16\" type=\"code_ptr\"/>"
                    "<reg name=\"hl\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"de\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"bc\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"af\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"ix\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"iy\" bitsize=\"16\" type=\"int\"/>"
/*                  "<reg name=\"s0\" bitsize=\"16\" type=\"int\"/>"  Need to update checksum!
                    "<reg name=\"s1\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"s2\" bitsize=\"16\" type=\"int\"/>"
                    "<reg name=\"s3\" bitsize=\"16\" type=\"int\"/>" */
                    "</feature><architecture>z80</architecture></target>#2f");
                return 1;
            }

            for (uint8_t i = 0; queries[i].request; i++)
            {
                if (strcmp(queries[i].request, payload) == 0)
                {
                    server_write_packet(queries[i].response);
                    return 1;
                }
            }

            goto error;
        }
        case '?':
        {
            // we're always stopped when we're under execution
            server_write_packet("T05thread:p01.01;");
            break;
        }
        case 'g':
        {
            // dump registers
            updateMappedPages();
            to_hex(gdbserver_state.registers, gdbserver_state.buffer, REGISTERS_COUNT * 2);
            write_packet_bytes(gdbserver_state.buffer, REGISTERS_COUNT * 4);
            break;
        }
        case 'G':
        {
            // set registers
            from_hex(payload, gdbserver_state.registers, REGISTERS_COUNT * 4);
            write_ok();
            break;
        }
        case 'P':
        {
            char* equals = __strchr(payload, '=');
            if (equals == NULL)
            {
                goto error;
            }

            /* parse register index as hex (numeric index only) */
            size_t name_len = (size_t)(equals - payload);
            if (name_len == 0 || name_len > 2)   /* allow 1-2 hex digits for index */
                goto error;

            uint16_t reg_index = (uint16_t)from_hex_str(payload, (uint8_t)name_len);
            if (reg_index >= REGISTERS_COUNT)   /* out of range */
                goto error;

            /* parse value (hex) after '=' */
            payload = equals + 1;
            uint16_t registerValue = (uint16_t)from_hex_str(payload, (uint8_t)4);
            gdbserver_state.registers[reg_index] = registerValue;

            write_ok();
            break;
        }
        case 'm':
        {
            // read memory
            // 8000,38

            char* comma = __strchr(payload, ',');
            if (comma == NULL)
            {
                goto error;
            }
            else
            {
                uint8_t* mem_offset = (uint8_t*)from_hex_str(payload, comma - payload);
                comma++;
                uint16_t mem_size = from_hex_str(comma, strlen(comma));
                if (mem_size > ((sizeof(gdbserver_state.buffer)-4) / 2))
                    goto error;

                to_hex(mem_offset, gdbserver_state.buffer, mem_size);
                write_packet_bytes(gdbserver_state.buffer, (uint8_t)(mem_size * 2));
            }
            break;
        }
        case 'M':
        {
            // write memory
            // 8000,38:<hex>

            char* comma = __strchr(payload, ',');
            if (comma == NULL)
            {
                goto error;
            }
            char* colon = __strchr(comma, ':');
            if (colon == NULL)
            {
                goto error;
            }

            uint8_t* mem_offset = (uint8_t*)from_hex_str(payload, comma - payload);
            uint16_t mem_size = from_hex_str(comma + 1, colon - comma - 1);
            from_hex(colon + 1, gdbserver_state.buffer, (uint8_t)(mem_size * 2));
            __memcpy((uint8_t*)mem_offset, gdbserver_state.buffer, mem_size);
            write_ok();
            break;
        }
        case 'Z':
        case 'z':
        {
            // place or delete a breakpoint

            // ignore type and expect comma after type
            payload++;
            if (*payload != ',')
            {
                goto error;
            }
            payload++;

            char* comma = __strchr(payload, ',');
            if (comma == NULL)
            {
                goto error;
            }

            uint16_t address = from_hex_str(payload, comma - payload);

            if (command == 'z')
            {
                for (uint8_t i = 0; i < MAX_BREAKPOINTS_COUNT; i++)
                {
                    struct breakpoint_t* b = &gdbserver_state.breakpoints[i];
                    if (b->address != address)
                    {
                        continue;
                    }

                    b->address = 0;
                    // restore original instruction
                    //*(uint8_t*)address = b->original_instruction;
                    write_ok();
                    return 1;
                }

                goto error;
            }
            else
            {
                for (uint8_t i = 0; i < MAX_BREAKPOINTS_COUNT; i++)
                {
                    //printS(">BP ADD<\r\n$");
                    struct breakpoint_t* b = &gdbserver_state.breakpoints[i];
                    if (b->address)
                    {
                        //printS(">FOUND BP<\r\n$");
                        continue;
                    }

                    b->address = address;
                    //b->original_instruction = *(uint8_t*)address;

                    //Only insert the RST08 if we are not already on this BP (otherwise it will be done using TRAP_FLAG_RESTORE_RST08H )
/*                    if (address != gdbserver_state.registers[REGISTERS_PC]){
                        *(uint8_t*)address = 0xCF; // RST 08h
                        if (*(uint8_t*)address != 0xCF)
                        {
                            // write didn't do anything, probably read only
                            b->address = 0;
                            goto error;
                        }
                    } */

                    write_ok();
                    return 1;
                }
                printS("[OUT OF BPS!]\r\n$");
                goto error;
            }


        }
        default:
        {
            goto error;
        }
    }

    return 1;
error:
    write_error();
    return 1;
}

static inline char serial_getc_blocking(void)
{
    return (char)dart_getc();
}

uint8_t server_read_data()
{
    char in = serial_getc_blocking();
    #ifdef DEBUG
    if (in != '$') 
        putchar((unsigned char)in);
    #endif
    switch (in)
    {
        case '$':
        {
            #ifdef DEBUG
            printf("\nIN > $");
            #endif
            uint8_t read_offset = 0;
            while(read_offset < sizeof(gdbserver_state.buffer) - 1)
            {
                in = serial_getc_blocking();
                #ifdef DEBUG
                putchar((unsigned char)in);
                #endif
                if (in == '#')
                {
                    gdbserver_state.buffer[read_offset] = 0;

                    uint8_t checksum_value = 0;
                    uint8_t sent_checksum = 0;

                    {
                        char checksum[2];
                        checksum[0] = serial_getc_blocking();
                        checksum[1] = serial_getc_blocking();

                        #ifdef DEBUG
                        putchar((unsigned char)checksum[0]);
                        putchar((unsigned char)checksum[1]);
                        #endif

                        {
                            uint8_t* buff = gdbserver_state.buffer;

                            for (uint8_t i = 0; i < read_offset; ++i)
                                checksum_value += *buff++;
                        }

                        from_hex(checksum, &sent_checksum, 2);
                    }

                    if (checksum_value == sent_checksum)
                    {
                        #ifdef DEBUG
                            printf("\nOUT> ");
                        #endif
                        dart_putc('+');              // ACK host packet                        
                        return process_packet();
                    }
                    else
                    {
                        #ifdef DEBUG
                        printf("\nERR> ");
                        #endif
                        printS("\r\n> GDB Checksum Error! <\r\n$");            
                        dart_putc('-');              // NACK host packet                        
                        write_error();
                    }

                    return 1;
                }
                else
                {
                    gdbserver_state.buffer[read_offset++] = in;
                }
            }
            // overflow
            printS("\r\n> GDB Buffer Overflow! <\r\n$");
            write_error();
            return 1;
        }
    }

    return 1;
}


__sfr __at (0xF0) SLOT_0_PORT;
__sfr __at (0xF1) SLOT_1_PORT;
__sfr __at (0xF2) SLOT_2_PORT;

void setPage(uint8_t slot, uint8_t page)
{
    switch (slot)
    {
        page = page | 0x80;
        case 0:
            SLOT_0_PORT = page;
            break;
        case 1:
            SLOT_1_PORT = page;
            break;
        case 2:
            SLOT_2_PORT = page;
            break;
    }
}

#define TEST_SLOT 0x0002u
#define FP_OFF 0x3FE8u   // last 8 bytes of a 16K slot window

void updateMappedPages()
{
    // 8-byte marker (includes NUL)
    static const uint8_t fingerprint[8] = { '>', 'Z', 'D', 'B', 'G', 'F', '?', '<' };

    // Address to check in TEST_SLOT 
    //volatile  uint8_t* fp_test = (uint8_t*)((uint16_t)(TEST_SLOT * 0x4000u + FP_OFF));

    for (uint8_t slot = 0; slot < 3; ++slot)
    {
        // Address to write in the currently mapped page for this slot
        volatile uint8_t* fp_orig = (uint8_t*)((uint16_t)(slot * 0x4000u + FP_OFF));

        uint8_t saved_bytes[8];
        __memcpy(saved_bytes, fp_orig, 8);
        __memcpy(fp_orig, fingerprint, 8);

        uint8_t slotFound = 0;

        // Cycle through mapping each physical page into TEST_SLOT (2)
        for (uint8_t page = 0; page < 128; ++page)
        {
            setPage(slot, page);

            // Compare exact 8 bytes at the same offset within the TEST_SLOT window
            uint8_t match = 1;
            for (uint8_t k = 0; k < 8; ++k) {
                if (fp_orig[k] != fingerprint[k]) { match = 0; break; }
            }

            if (match)
            {
                // Record the 7-bit page number (hardware typically uses bit7 as map-enable)
                gdbserver_state.registers[SLOT_0 + slot] = (uint16_t)page;
                // restore original contents
                __memcpy(fp_orig, saved_bytes, 8);
                slotFound = 1;
                break;
            }
        }
        if (slotFound == 0)
        {
            // Not found, mark as invalid - we have messed with the memory as we couldt restore the fingerprint or the page!
            gdbserver_state.registers[SLOT_0 + slot] = 0xFFFF;
        }
    }
    // Slot 2 should now contain the original contents of slot 2

    // Slot 3 is fixed
    gdbserver_state.registers[SLOT_3] = 7;
}
