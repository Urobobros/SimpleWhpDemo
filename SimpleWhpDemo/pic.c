#include "pic.h"

UCHAR PicMasterImr = 0;
UCHAR PicSlaveImr = 0;

#include "io.h"
#include <stddef.h>

static uint8_t PicRead(USHORT port, void *priv)
{
    (void)priv;
    switch(port)
    {
    case 0x21: return PicMasterImr;
    case 0xA1: return PicSlaveImr;
    default: return 0;
    }
}

static void PicWrite(USHORT port, UCHAR val, void *priv)
{
    (void)priv;
    switch(port)
    {
    case 0x20:
    case 0x21:
        PicMasterImr = val;
        break;
    case 0xA0:
    case 0xA1:
        PicSlaveImr = val;
        break;
    default:
        break;
    }
}

void PicInit(void)
{
    io_sethandler(0x0020, 0x0002, PicRead, NULL, NULL, PicWrite, NULL, NULL, NULL);
    io_sethandler(0x00A0, 0x0002, PicRead, NULL, NULL, PicWrite, NULL, NULL, NULL);
}
