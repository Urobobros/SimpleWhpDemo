#include "cga.h"

UCHAR CgaMode = 0;
UCHAR AttrCga = 0;
UCHAR CgaStatus = 0;
ULONGLONG CgaLastToggleMs = 0;

void CgaOut(USHORT port, UCHAR val)
{
    switch (port)
    {
    case 0x3D8:
        CgaMode = val;
        break;
    case 0x3D9:
        AttrCga = val;
        break;
    default:
        break;
    }
}
