#include "fdc.h"
#include "io.h"
#include <stddef.h>

static UCHAR FdcDor, FdcStatus, FdcData;

static uint8_t fdc_read(uint16_t port, void *priv)
{
    (void)priv;
    switch(port) {
    case 0x03F2: return FdcDor;
    case 0x03F4: return FdcStatus;
    case 0x03F5: return FdcData;
    default: return 0x00;
    }
}

static void fdc_write(uint16_t port, uint8_t val, void *priv)
{
    (void)priv;
    switch(port) {
    case 0x03F2: FdcDor = val; break;
    case 0x03F4: FdcStatus = val; break;
    case 0x03F5: FdcData = val; break;
    default: break;
    }
}

void fdc_add(void)
{
    io_sethandler(0x03f0, 0x0006, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
    io_sethandler(0x03f7, 0x0001, fdc_read, NULL, NULL, fdc_write, NULL, NULL, NULL);
}
