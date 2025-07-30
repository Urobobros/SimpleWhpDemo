#include "keyboard.h"
#include "io.h"
#include <stdio.h>

static UCHAR pb = 0x40; // PC/XT PPI port B initial state
static UCHAR pa = 0;
static UCHAR key_queue[16];
static int queue_start = 0, queue_end = 0;
static int port62_reads = 0;
static int port62_ack_count = 0;

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
    port62_reads = 0;
    port62_ack_count = 0;
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
    case 0x63:
        return pb;
    case 0x62:
        if (port62_ack_count > 0)
        {
            port62_ack_count--;
            return 0x26;
        }
        if (port62_reads < 4)
        {
            port62_reads++;
            return 0x2D;
        }
        return 0x00;
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
    case 0x63:
        if ((pb & 0x40) == 0 && (val & 0x40))
        {
            queue_start = queue_end = 0;
            KeyboardAddData(0xaa);
        }
        if (val == 0xA9)
        {
            port62_ack_count = 4;
            port62_reads = 0;
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

static uint8_t keyboard_inb(uint16_t port, void *p)
{
    (void)p;
    return KeyboardXtRead(port);
}

static void keyboard_outb(uint16_t port, uint8_t val, void *p)
{
    (void)p;
    KeyboardXtWrite(port, val);
}

void KeyboardXtInit(void)
{
    KeyboardInit();
    io_sethandler(0x0060, 0x0004, keyboard_inb, NULL, NULL, keyboard_outb, NULL, NULL, NULL);
}
