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
    switch(port)
    {
    case 0x60:
        return KeyboardReadData();
    case 0x61:
        return pb;
    case 0x62:
        return 0x2D;
    default:
        return 0xFF;
    }
}

void KeyboardWrite(USHORT port, UCHAR val)
{
    switch(port)
    {
    case 0x60:
        pa = val;
        break;
    case 0x61:
        if((pb & 0x40) == 0 && (val & 0x40))
        {
            /* keyboard reset ack */
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

