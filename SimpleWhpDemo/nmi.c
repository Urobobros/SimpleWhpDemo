#include "nmi.h"

UCHAR NmiMask = 0;

void NmiWrite(USHORT port, UCHAR val)
{
    (void)port;
    NmiMask = val & 0x80;
}

