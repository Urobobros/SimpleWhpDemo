#include "keyboard.h"
#include <stdio.h>
static UCHAR pb = 0;

UCHAR KeyboardReadData(void)
{
    int ch = getchar();
    return (UCHAR)ch;
}

UCHAR KeyboardReadStatus(void)
{
    return 0;
}

void KeyboardWrite(USHORT port, UCHAR val)
{
    if(port == 0x61)
    {
        if((pb & 0x40) == 0 && (val & 0x40))
        {
            /* keyboard reset ack */
        }
        pb = val;
    }
    (void)port;
}
