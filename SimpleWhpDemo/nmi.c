#include "nmi.h"
#include "io.h"
#include <stddef.h>

UCHAR NmiMask = 0;

void NmiWrite(USHORT port, UCHAR val)
{
    (void)port;
    NmiMask = val & 0x80;
}

static void nmi_write_cb(uint16_t port, uint8_t val, void *priv)
{
    (void)priv;
    NmiWrite(port, val);
}

void NmiInit(void)
{
    io_sethandler(0x00A0, 0x0001, NULL, NULL, NULL, nmi_write_cb, NULL, NULL, NULL);
    NmiMask = 0;
}

