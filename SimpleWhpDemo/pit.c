#include "pit.h"
#include "io.h"
#include <string.h>

PIT pit, pit2;

static uint8_t pit_channel_read(PIT *p, int idx)
{
    PIT_CHANNEL *ch = &p->ch[idx];
    uint32_t val = ch->latched ? ch->latch : ch->count;
    if (!val)
        val = 0x10000;
    uint8_t out;
    switch (ch->access) {
    case 2:
        out = (val >> 8) & 0xFF;
        break;
    case 3:
        if (ch->rw_low) {
            out = val & 0xFF;
            ch->rw_low = 0;
        } else {
            out = (val >> 8) & 0xFF;
            ch->rw_low = 1;
            ch->latched = 0;
        }
        break;
    default:
        out = val & 0xFF;
        ch->latched = 0;
        break;
    }
    if (ch->access != 3)
        ch->latched = 0;
    return out;
}

static void pit_channel_write(PIT *p, int idx, uint8_t val)
{
    PIT_CHANNEL *ch = &p->ch[idx];
    switch (ch->access) {
    case 1:
        ch->reload = val;
        ch->count = ch->reload ? ch->reload : 0x10000;
        break;
    case 2:
        ch->reload = ((uint16_t)val) << 8;
        ch->count = ch->reload ? ch->reload : 0x10000;
        break;
    case 3:
        if (ch->rw_low) {
            ch->reload = (ch->reload & 0xFF00) | val;
            ch->rw_low = 0;
        } else {
            ch->reload = (ch->reload & 0x00FF) | ((uint16_t)val << 8);
            ch->count = ch->reload ? ch->reload : 0x10000;
            ch->rw_low = 1;
        }
        break;
    default:
        ch->reload = (ch->reload & 0xFF00) | val;
        ch->count = ch->reload ? ch->reload : 0x10000;
        break;
    }
}

uint8_t pit_read(uint16_t port, void *priv)
{
    PIT *p = (PIT*)priv;
    int idx = port & 3;
    if (idx < 3)
        return pit_channel_read(p, idx);
    return p->control;
}

void pit_write(uint16_t port, uint8_t val, void *priv)
{
    PIT *p = (PIT*)priv;
    int idx = port & 3;
    if (idx == 3) {
        p->control = val;
        if ((val & 0x30) == 0) {
            int ch = (val >> 6) & 3;
            if (ch < 3) {
                PIT_CHANNEL *c = &p->ch[ch];
                c->latch = c->count;
                c->latched = 1;
                c->rw_low = 1;
            }
        } else {
            int ch = (val >> 6) & 3;
            if (ch < 3) {
                PIT_CHANNEL *c = &p->ch[ch];
                c->access = (val >> 4) & 3;
                c->mode = (val >> 1) & 7;
                c->bcd = val & 1;
                c->rw_low = 1;
            }
        }
    } else {
        pit_channel_write(p, idx, val);
    }
}

void pit_init(void)
{
    memset(&pit, 0, sizeof(pit));
    for (int i = 0; i < 3; i++) {
        pit.ch[i].count = 0xFFFF;
        pit.ch[i].reload = 0xFFFF;
        pit.ch[i].access = 3;
        pit.ch[i].rw_low = 1;
    }
    io_sethandler(0x0040, 0x0004, pit_read, NULL, NULL, pit_write, NULL, NULL, &pit);
}

void pit_ps2_init(void)
{
    memset(&pit2, 0, sizeof(pit2));
    for (int i = 0; i < 3; i++) {
        pit2.ch[i].count = 0xFFFF;
        pit2.ch[i].reload = 0xFFFF;
        pit2.ch[i].access = 3;
        pit2.ch[i].rw_low = 1;
    }
    io_sethandler(0x0044, 0x0001, pit_read, NULL, NULL, pit_write, NULL, NULL, &pit2);
    io_sethandler(0x0047, 0x0001, pit_read, NULL, NULL, pit_write, NULL, NULL, &pit2);
}
