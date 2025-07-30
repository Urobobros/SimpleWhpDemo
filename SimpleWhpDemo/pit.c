#include "pit.h"
#include "io.h"
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

PIT pit, pit2;
static uint64_t pit_last_update_us;
static double pit_partial_ticks;

#define PIT_FREQUENCY 1193182

static uint64_t pit_now_us(void)
{
#ifdef _WIN32
    return GetTickCount64() * 1000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
#endif
}

#ifdef IO_TEST_ACCESS
void pit_tick_channel(PIT_CHANNEL *ch, int *out, PITOutFunc func, uint64_t ticks)
#else
static void pit_tick_channel(PIT_CHANNEL *ch, int *out, PITOutFunc func, uint64_t ticks)
#endif
{
    uint32_t reload = ch->reload ? ch->reload : 0x10000u;
    uint32_t count = ch->count ? ch->count : reload;
    while (ticks > 0) {
        if (ticks >= count) {
            ticks -= count;
            count = reload;
            int old = *out;
            *out ^= 1;
            if (func && old != *out)
                func(*out, old);
        } else {
            count -= (uint32_t)ticks;
            ticks = 0;
        }
    }
    ch->count = count;
}

void pit_update(PIT *p)
{
    uint64_t now = pit_now_us();
    if (!pit_last_update_us) {
        pit_last_update_us = now;
        return;
    }
    double elapsed = (now - pit_last_update_us) / 1000000.0;
    pit_partial_ticks += elapsed * (double)PIT_FREQUENCY;
    uint64_t ticks = (uint64_t)pit_partial_ticks;
    if (!ticks)
        return;

    pit_partial_ticks -= (double)ticks;
    pit_last_update_us = now;

    for (int i = 0; i < 3; i++) {
        pit_tick_channel(&p->ch[i], &p->out[i], p->out_func[i], ticks);
    }
}

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
        pit.out[i] = 0;
        pit.out_func[i] = NULL;
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
        pit2.out[i] = 0;
        pit2.out_func[i] = NULL;
    }
    io_sethandler(0x0044, 0x0001, pit_read, NULL, NULL, pit_write, NULL, NULL, &pit2);
    io_sethandler(0x0047, 0x0001, pit_read, NULL, NULL, pit_write, NULL, NULL, &pit2);
}

void pit_set_out_func(PIT *p, int chan, PITOutFunc func)
{
    if (chan >=0 && chan < 3)
        p->out_func[chan] = func;
}
