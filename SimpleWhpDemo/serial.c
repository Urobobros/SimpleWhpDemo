#include "serial.h"

#define COM1_DATA 0x03F8
#define COM1_IER  0x03F9
#define COM1_IIR  0x03FA
#define COM1_LCR  0x03FB
#define COM1_MCR  0x03FC
#define COM1_LSR  0x03FD
#define COM1_MSR  0x03FE
#define COM1_SCR  0x03FF

typedef struct {
    UCHAR thr;
    UCHAR ier;
    UCHAR lcr;
    UCHAR dll;
    UCHAR dlm;
} SERIAL_PORT;

static SERIAL_PORT Com1, Com2;

#include "io.h"
#include <stddef.h>

static void serial_reset(SERIAL_PORT *s)
{
    s->thr = s->ier = s->lcr = 0;
    s->dll = s->dlm = 0;
}

void SerialInit(void)
{
    serial_reset(&Com1);
    serial_reset(&Com2);
}

static void serial_write(uint16_t port, uint8_t val, void *p)
{
    SERIAL_PORT *s = (SERIAL_PORT*)p;
    switch (port & 7) {
    case 0:
        if (s->lcr & 0x80)
            s->dll = val;
        else
            s->thr = val;
        break;
    case 1:
        if (s->lcr & 0x80)
            s->dlm = val;
        else
            s->ier = val;
        break;
    case 3:
        s->lcr = val;
        break;
    default:
        break;
    }
}

static uint8_t serial_read(uint16_t port, void *p)
{
    SERIAL_PORT *s = (SERIAL_PORT*)p;
    switch (port & 7) {
    case 0:
        return (s->lcr & 0x80) ? s->dll : s->thr;
    case 1:
        return (s->lcr & 0x80) ? s->dlm : s->ier;
    case 2:
        return 0x01; /* no interrupts pending */
    case 3:
        return s->lcr;
    case 5:
        return 0x60; /* THR empty */
    default:
        return 0;
    }
}

void SerialWrite(USHORT port, UCHAR val)
{
    if (port >= COM1_DATA && port <= COM1_SCR)
        serial_write(port, val, &Com1);
    else if (port >= 0x02F8 && port <= 0x02FF)
        serial_write(port, val, &Com2);
}

UCHAR SerialRead(USHORT port)
{
    if (port >= COM1_DATA && port <= COM1_SCR)
        return serial_read(port, &Com1);
    else if (port >= 0x02F8 && port <= 0x02FF)
        return serial_read(port, &Com2);
    return 0xFF;
}

void serial1_init(uint16_t addr, int irq, int has_fifo)
{
    (void)irq; (void)has_fifo;
    serial_reset(&Com1);
    io_sethandler(addr, 0x0008, serial_read, NULL, NULL, serial_write, NULL, NULL, &Com1);
}

void serial2_init(uint16_t addr, int irq, int has_fifo)
{
    (void)irq; (void)has_fifo;
    serial_reset(&Com2);
    io_sethandler(addr, 0x0008, serial_read, NULL, NULL, serial_write, NULL, NULL, &Com2);
}
