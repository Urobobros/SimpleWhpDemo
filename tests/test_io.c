#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

static UCHAR bval;
static USHORT wval;
static uint32_t lval;

static uint8_t inb_fn(uint16_t addr, void *priv) { (void)addr; return bval; }
static uint16_t inw_fn(uint16_t addr, void *priv) { (void)addr; return wval; }
static uint32_t inl_fn(uint16_t addr, void *priv) { (void)addr; return lval; }
static void outb_fn(uint16_t addr, uint8_t val, void *priv) { (void)addr; bval = val; }
static void outw_fn(uint16_t addr, uint16_t val, void *priv) { (void)addr; wval = val; }
static void outl_fn(uint16_t addr, uint32_t val, void *priv) { (void)addr; lval = val; }

int main(void)
{
    io_init();
    io_sethandler(0x1000, 1, inb_fn, inw_fn, inl_fn, outb_fn, outw_fn, outl_fn, NULL);

    outb(0x1000, 0x5A);
    assert(bval == 0x5A);
    bval = 0xA5;
    assert(inb(0x1000) == 0xA5);

    outw(0x1000, 0x1234);
    assert(wval == 0x1234);
    wval = 0x4321;
    assert(inw(0x1000) == 0x4321);

    outl(0x1000, 0x89ABCDEF);
    assert(lval == 0x89ABCDEF);
    lval = 0x0FEDCBA9;
    assert(inl(0x1000) == 0x0FEDCBA9);

    puts("IO test OK");
    return 0;
}
