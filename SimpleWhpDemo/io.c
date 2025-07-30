#include "io.h"
#include "include/portlog.h"
#include <stdio.h>
#include <string.h>

#define COMBINE_NAMES(buf, name1, name2) \
    do { \
        if ((name1) && (name2)) \
            snprintf(buf, sizeof(buf), "%s/%s", name1, name2); \
        else if (name1) \
            snprintf(buf, sizeof(buf), "%s", name1); \
        else if (name2) \
            snprintf(buf, sizeof(buf), "%s", name2); \
        else \
            snprintf(buf, sizeof(buf), "unhandled"); \
    } while (0)

#ifdef IO_TEST_ACCESS
uint8_t (*port_inb[0x10000][2])(uint16_t, void*);
uint16_t (*port_inw[0x10000][2])(uint16_t, void*);
uint32_t (*port_inl[0x10000][2])(uint16_t, void*);
#else
static uint8_t (*port_inb[0x10000][2])(uint16_t, void*);
static uint16_t (*port_inw[0x10000][2])(uint16_t, void*);
static uint32_t (*port_inl[0x10000][2])(uint16_t, void*);
#endif

#ifdef IO_TEST_ACCESS
void (*port_outb[0x10000][2])(uint16_t, uint8_t, void*);
void (*port_outw[0x10000][2])(uint16_t, uint16_t, void*);
void (*port_outl[0x10000][2])(uint16_t, uint32_t, void*);
#else
static void (*port_outb[0x10000][2])(uint16_t, uint8_t, void*);
static void (*port_outw[0x10000][2])(uint16_t, uint16_t, void*);
static void (*port_outl[0x10000][2])(uint16_t, uint32_t, void*);
#endif

static const char *port_inb_name[0x10000][2];
static const char *port_inw_name[0x10000][2];
static const char *port_inl_name[0x10000][2];
static const char *port_outb_name[0x10000][2];
static const char *port_outw_name[0x10000][2];
static const char *port_outl_name[0x10000][2];

#ifdef IO_TEST_ACCESS
void *port_priv[0x10000][2];
#else
static void *port_priv[0x10000][2];
#endif

void io_init(void)
{
    for (int i = 0; i < 0x10000; i++) {
        for (int j = 0; j < 2; j++) {
            port_inb[i][j] = NULL;
            port_inw[i][j] = NULL;
            port_inl[i][j] = NULL;
            port_outb[i][j] = NULL;
            port_outw[i][j] = NULL;
            port_outl[i][j] = NULL;
            port_inb_name[i][j] = NULL;
            port_inw_name[i][j] = NULL;
            port_inl_name[i][j] = NULL;
            port_outb_name[i][j] = NULL;
            port_outw_name[i][j] = NULL;
            port_outl_name[i][j] = NULL;
            port_priv[i][j] = NULL;
        }
    }
}

void io_sethandler_named(uint16_t base, int size,
                         uint8_t (*inb_fn)(uint16_t, void*),
                         uint16_t (*inw_fn)(uint16_t, void*),
                         uint32_t (*inl_fn)(uint16_t, void*),
                         void (*outb_fn)(uint16_t, uint8_t, void*),
                         void (*outw_fn)(uint16_t, uint16_t, void*),
                         void (*outl_fn)(uint16_t, uint32_t, void*),
                         void *priv,
                         const char *inb_name, const char *inw_name,
                         const char *inl_name, const char *outb_name,
                         const char *outw_name, const char *outl_name)
{
    for (int c = 0; c < size; c++) {
        if (!port_inb[base+c][0] && !port_inw[base+c][0] && !port_inl[base+c][0] &&
            !port_outb[base+c][0] && !port_outw[base+c][0] && !port_outl[base+c][0]) {
            port_inb[base+c][0] = inb_fn;
            port_inw[base+c][0] = inw_fn;
            port_inl[base+c][0] = inl_fn;
            port_outb[base+c][0] = outb_fn;
            port_outw[base+c][0] = outw_fn;
            port_outl[base+c][0] = outl_fn;
            port_inb_name[base+c][0] = inb_name;
            port_inw_name[base+c][0] = inw_name;
            port_inl_name[base+c][0] = inl_name;
            port_outb_name[base+c][0] = outb_name;
            port_outw_name[base+c][0] = outw_name;
            port_outl_name[base+c][0] = outl_name;
            port_priv[base+c][0] = priv;
        } else if (!port_inb[base+c][1] && !port_inw[base+c][1] && !port_inl[base+c][1] &&
                   !port_outb[base+c][1] && !port_outw[base+c][1] && !port_outl[base+c][1]) {
            port_inb[base+c][1] = inb_fn;
            port_inw[base+c][1] = inw_fn;
            port_inl[base+c][1] = inl_fn;
            port_outb[base+c][1] = outb_fn;
            port_outw[base+c][1] = outw_fn;
            port_outl[base+c][1] = outl_fn;
            port_inb_name[base+c][1] = inb_name;
            port_inw_name[base+c][1] = inw_name;
            port_inl_name[base+c][1] = inl_name;
            port_outb_name[base+c][1] = outb_name;
            port_outw_name[base+c][1] = outw_name;
            port_outl_name[base+c][1] = outl_name;
            port_priv[base+c][1] = priv;
        }
    }
}

void io_removehandler(uint16_t base, int size,
                      uint8_t (*inb_fn)(uint16_t, void*),
                      uint16_t (*inw_fn)(uint16_t, void*),
                      uint32_t (*inl_fn)(uint16_t, void*),
                      void (*outb_fn)(uint16_t, uint8_t, void*),
                      void (*outw_fn)(uint16_t, uint16_t, void*),
                      void (*outl_fn)(uint16_t, uint32_t, void*),
                      void *priv)
{
    for (int c = 0; c < size; c++) {
        if (port_priv[base+c][0] == priv &&
            port_inb[base+c][0] == inb_fn && port_inw[base+c][0] == inw_fn &&
            port_inl[base+c][0] == inl_fn && port_outb[base+c][0] == outb_fn &&
            port_outw[base+c][0] == outw_fn && port_outl[base+c][0] == outl_fn) {
            port_inb[base+c][0] = NULL;
            port_inw[base+c][0] = NULL;
            port_inl[base+c][0] = NULL;
            port_outb[base+c][0] = NULL;
            port_outw[base+c][0] = NULL;
            port_outl[base+c][0] = NULL;
            port_inb_name[base+c][0] = NULL;
            port_inw_name[base+c][0] = NULL;
            port_inl_name[base+c][0] = NULL;
            port_outb_name[base+c][0] = NULL;
            port_outw_name[base+c][0] = NULL;
            port_outl_name[base+c][0] = NULL;
            port_priv[base+c][0] = NULL;
        }
        if (port_priv[base+c][1] == priv &&
            port_inb[base+c][1] == inb_fn && port_inw[base+c][1] == inw_fn &&
            port_inl[base+c][1] == inl_fn && port_outb[base+c][1] == outb_fn &&
            port_outw[base+c][1] == outw_fn && port_outl[base+c][1] == outl_fn) {
            port_inb[base+c][1] = NULL;
            port_inw[base+c][1] = NULL;
            port_inl[base+c][1] = NULL;
            port_outb[base+c][1] = NULL;
            port_outw[base+c][1] = NULL;
            port_outl[base+c][1] = NULL;
            port_inb_name[base+c][1] = NULL;
            port_inw_name[base+c][1] = NULL;
            port_inl_name[base+c][1] = NULL;
            port_outb_name[base+c][1] = NULL;
            port_outw_name[base+c][1] = NULL;
            port_outl_name[base+c][1] = NULL;
            port_priv[base+c][1] = NULL;
        }
    }
}

uint8_t inb(uint16_t port)
{
    uint8_t val = 0xFF;
    if (port_inb[port][0])
        val &= port_inb[port][0](port, port_priv[port][0]);
    if (port_inb[port][1])
        val &= port_inb[port][1](port, port_priv[port][1]);

    char nbuf[64];
    COMBINE_NAMES(nbuf, port_inb_name[port][0], port_inb_name[port][1]);
    PORT_LOG("IN  port 0x%04X, size 1, value 0x%02X  # %s\n", port, val, nbuf);
    return val;
}

void outb(uint16_t port, uint8_t val)
{
    char nbuf[64];
    COMBINE_NAMES(nbuf, port_outb_name[port][0], port_outb_name[port][1]);
    PORT_LOG("OUT port 0x%04X, size 1, value 0x%02X  # %s\n", port, val, nbuf);
    if (port_outb[port][0])
        port_outb[port][0](port, val, port_priv[port][0]);
    if (port_outb[port][1])
        port_outb[port][1](port, val, port_priv[port][1]);
}

uint16_t inw(uint16_t port)
{
    uint16_t val;
    if (port_inw[port][0])
        val = port_inw[port][0](port, port_priv[port][0]);
    else if (port_inw[port][1])
        val = port_inw[port][1](port, port_priv[port][1]);
    else
        val = inb(port) | (inb(port+1) << 8);

    char nbuf[64];
    COMBINE_NAMES(nbuf, port_inw_name[port][0], port_inw_name[port][1]);
    PORT_LOG("IN  port 0x%04X, size 2, value 0x%04X  # %s\n", port, val, nbuf);
    return val;
}

void outw(uint16_t port, uint16_t val)
{
    char nbuf[64];
    COMBINE_NAMES(nbuf, port_outw_name[port][0], port_outw_name[port][1]);
    PORT_LOG("OUT port 0x%04X, size 2, value 0x%04X  # %s\n", port, val, nbuf);
    if (port_outw[port][0])
        port_outw[port][0](port, val, port_priv[port][0]);
    if (port_outw[port][1])
        port_outw[port][1](port, val, port_priv[port][1]);
    if (!port_outw[port][0] && !port_outw[port][1]) {
        outb(port, val & 0xFF);
        outb(port+1, val >> 8);
    }
}

uint32_t inl(uint16_t port)
{
    uint32_t val;
    if (port_inl[port][0])
        val = port_inl[port][0](port, port_priv[port][0]);
    else if (port_inl[port][1])
        val = port_inl[port][1](port, port_priv[port][1]);
    else
        val = inw(port) | ((uint32_t)inw(port+2) << 16);

    char nbuf[64];
    COMBINE_NAMES(nbuf, port_inl_name[port][0], port_inl_name[port][1]);
    PORT_LOG("IN  port 0x%04X, size 4, value 0x%08X  # %s\n", port, val, nbuf);
    return val;
}

void outl(uint16_t port, uint32_t val)
{
    char nbuf[64];
    COMBINE_NAMES(nbuf, port_outl_name[port][0], port_outl_name[port][1]);
    PORT_LOG("OUT port 0x%04X, size 4, value 0x%08X  # %s\n", port, val, nbuf);
    if (port_outl[port][0])
        port_outl[port][0](port, val, port_priv[port][0]);
    if (port_outl[port][1])
        port_outl[port][1](port, val, port_priv[port][1]);
    if (!port_outl[port][0] && !port_outl[port][1]) {
        outw(port, val & 0xFFFF);
        outw(port+2, val >> 16);
    }
}

int io_has_handler(uint16_t port, int is_write)
{
    if (is_write) {
        return port_outb[port][0] || port_outb[port][1] ||
               port_outw[port][0] || port_outw[port][1] ||
               port_outl[port][0] || port_outl[port][1];
    } else {
        return port_inb[port][0] || port_inb[port][1] ||
               port_inw[port][0] || port_inw[port][1] ||
               port_inl[port][0] || port_inl[port][1];
    }
}

