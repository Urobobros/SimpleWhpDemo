#include "cga.h"
#ifndef _WIN32
#include <sys/time.h>
#endif

UCHAR CgaMode = 0;
UCHAR AttrCga = 0;
UCHAR CgaStatus = 0;
ULONGLONG CgaLastToggleMs = 0;
UCHAR CrtcCgaIndex = 0;
UCHAR CrtcCgaRegs[32] = {0};

void CgaOut(USHORT port, UCHAR val)
{
    switch (port)
    {
    case 0x3D4:
    case 0x3D6:
        CrtcCgaIndex = val & 0x1F;
        break;
    case 0x3D5:
    case 0x3D7:
        CrtcCgaRegs[CrtcCgaIndex] = val;
        break;
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

static ULONGLONG cga_now_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (ULONGLONG)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
#endif
}

void CgaInit(void)
{
    CgaMode = AttrCga = 0;
    CgaStatus = 0;
    CgaLastToggleMs = cga_now_ms();
}

UCHAR CgaIn(USHORT port)
{
    if (port == 0x3DA)
    {
        ULONGLONG now = cga_now_ms();
        /* Simulate display enable toggling at ~60 Hz */
        if (now - CgaLastToggleMs >= 16)
        {
            CgaStatus ^= 0x01;
            CgaLastToggleMs += 16;
        }
        /* Keep vertical retrace bit simple: toggle every 1 ms */
        if (((now / 1) & 1) != 0)
            CgaStatus |= 0x08;
        else
            CgaStatus &= ~0x08;

        return CgaStatus;
    }
    else if (port == 0x3D4)
    {
        return CrtcCgaIndex;
    }
    else if (port == 0x3D5)
    {
        return CrtcCgaRegs[CrtcCgaIndex];
    }
    return 0xFF;
}
