#include "keyboard.h"
#include <stdio.h>

static UCHAR pb = 0x40; // PC/XT PPI port B initial state
static UCHAR pa = 0;
static UCHAR key_queue[16];
static int queue_start = 0, queue_end = 0;

static void KeyboardAddData(UCHAR val)
{
    int next = (queue_end + 1) & 0xF;
    if (next != queue_start)
    {
        key_queue[queue_end] = val;
        queue_end = next;
    }
}

void KeyboardInit(void)
{
    pb = 0x40; // match PCem's initial PB value
    pa = 0;
    queue_start = queue_end = 0;
}

UCHAR KeyboardReadData(void)
{
    if (queue_start != queue_end)
    {
        UCHAR val = key_queue[queue_start];
        queue_start = (queue_start + 1) & 0xF;
        return val;
    }

    int ch = getchar();
    return (UCHAR)ch;
}

UCHAR KeyboardReadStatus(void)
{
    return (queue_start != queue_end) ? 1 : 0;
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
        if ((pb & 0x40) == 0 && (val & 0x40))
        {
            queue_start = queue_end = 0;
            KeyboardAddData(0xaa);
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
