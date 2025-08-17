#include "utils.h"
#include <stddef.h>

#ifdef TARGET_PCW_DART
// C fallbacks for hex helpers when not using ZX assembly versions
static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

uint8_t hex_to_char(const char* from)
{
    uint8_t hi = hex_nibble(from[0]);
    uint8_t lo = hex_nibble(from[1]);
    return (uint8_t)((hi << 4) | lo);
}

void char_to_hex(char* res, uint8_t c)
{
    static const char hex[] = "0123456789abcdef";
    res[0] = hex[(c >> 4) & 0x0F];
    res[1] = hex[c & 0x0F];
}
#endif

void from_hex(const char* in, uint8_t* out, uint8_t len)
{
    const char* end = in + len;
    while (in < end)
    {
        *out++ = hex_to_char(in);
        in += 2;
    }
}

void to_hex(const uint8_t* in, char* out, uint8_t len)
{
    while (len--)
    {
        char_to_hex(out, *in++);
        out += 2;
    }
}

void *__memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

void *__memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

uint16_t from_hex_str(const char* in, uint8_t len)
{
    char b[4];
    __memset(b, '0', 4);
    __memcpy(b + 4 - len, in, len);

    uint8_t result[2];
    from_hex(b, result, 4);

    uint16_t result_v;

    *(uint8_t*)&result_v = result[1];
    *((uint8_t*)&result_v + 1) = result[0];

    return result_v;
}