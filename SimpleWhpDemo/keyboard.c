#include "keyboard.h"
#include <stdio.h>

static UCHAR pb = 0;
static UCHAR pa = 0;

void KeyboardInit(void)
{
    pb = 0;
    pa = 0;
}

UCHAR KeyboardReadData(void)
{
    int ch = getchar();
    return (UCHAR)ch;
}

UCHAR KeyboardReadStatus(void)
{
    return 0;
}

UCHAR KeyboardXtRead(USHORT port)
{
    switch (port)
    {
    case 0x60:
        return KeyboardReadData();
    case 0x61:
        return pb;
    case 0x62:
        return 0x2D; // typická návratová hodnota XT BIOSu
    case 0x63:
        return pb;   // fallback – některé klony používají 0x63 místo 0x61
    default:
        return 0xFF;
    }
}

void KeyboardWrite(USHORT port, UCHAR val)
{
    switch (port)
    {
    case 0x60:
        pa = val;
        break;
    case 0x61:
    case 0x63: // XT klony často používají 0x63 stejně jako 0x61
        if ((pb & 0x40) == 0 && (val & 0x40))
        {
            // keyboard reset ack – zatím neimplementováno
        }
        pb = val;
        break;
    default:
        break;
    }
}

void KeyboardXtWrite(USHORT port, UCHAR val)
{
    KeyboardWrite(port, val);
}
