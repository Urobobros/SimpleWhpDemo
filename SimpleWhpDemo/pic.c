#include "pic.h"

UCHAR PicMasterImr = 0;
UCHAR PicSlaveImr = 0;

void PicWrite(USHORT port, UCHAR val)
{
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
