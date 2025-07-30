#include "windows.h"
#include "../SimpleWhpDemo/serial.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    SerialInit();
    SerialWrite(0x03FB, 0x80); /* enable DLAB */
    SerialWrite(0x03F8, 0x34);
    SerialWrite(0x03F9, 0x12);
    assert(SerialRead(0x03F8) == 0x34);
    assert(SerialRead(0x03F9) == 0x12);
    SerialWrite(0x03FB, 0x00); /* disable DLAB */
    SerialWrite(0x03F8, 0x55);
    assert(SerialRead(0x03F8) == 0x55);
    assert(SerialRead(0x03FD) == 0x60);
    puts("Serial test OK");
    return 0;
}
