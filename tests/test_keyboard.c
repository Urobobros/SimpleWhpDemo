#include "windows.h"
#include "../SimpleWhpDemo/keyboard.h"
#include <stdio.h>
#include <assert.h>

int main(void)
{
    KeyboardInit();

    KeyboardXtWrite(0x0063, 0x99);          // inicializace
    KeyboardXtWrite(0x0061, 0xA1);          // zápis na port 0x61
    printf("0x0061 = %02X\n", KeyboardXtRead(0x0061));
    assert(KeyboardXtRead(0x0061) == 0xA1);

    KeyboardXtWrite(0x0061, 0xB1);
    KeyboardXtWrite(0x0061, 0x81);

    for (int i = 0; i < 4; i++) {
        UCHAR v = KeyboardXtRead(0x0062);
        printf("0x0062 = %02X\n", v);
        assert(v == 0x2D);
    }
    UCHAR v = KeyboardXtRead(0x0062);
    printf("0x0062 = %02X\n", v);
    assert(v == 0x00);

    KeyboardXtWrite(0x0061, 0xA9);
    v = KeyboardXtRead(0x0062);
    printf("0x0062 = %02X\n", v);
    assert(v == 0x26);
    assert(KeyboardXtRead(0x0061) == 0xA9);
    KeyboardXtWrite(0x0061, 0xB9);
    KeyboardXtWrite(0x0061, 0x89);
    for (int i = 0; i < 3; i++) {
        v = KeyboardXtRead(0x0062);
        printf("0x0062 = %02X\n", v);
        assert(v == 0x26);
    }
    // verify reset transition adds 0xAA to data queue
    KeyboardXtWrite(0x0061, 0x00); // ensure bit 6 = 0
    KeyboardXtWrite(0x0061, 0xC0); // make bit 6 = 1 with bit7=1
    KeyboardXtWrite(0x0061, 0x40); // clear bit7 but keep bit6 = 1
    v = KeyboardXtRead(0x0060);
    printf("0x0060 = %02X\n", v);
    assert(v == 0xAA);

    puts("Keyboard test OK");
    return 0;
}
