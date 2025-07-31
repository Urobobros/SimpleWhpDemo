#include "dma.h"

UCHAR DmaTemp = 0;
UCHAR DmaMode = 0;
UCHAR DmaMask = 0;
UCHAR DmaClear = 0;
UCHAR DmaPage[16] = {0};
#define DmaPage1 (DmaPage[1])
#define DmaPage3 (DmaPage[3])
USHORT DmaAddr[4] = {0};
USHORT DmaCount[4] = {0};
BOOL DmaFlipFlop = FALSE;

#include "io.h"
#include <stddef.h>

static uint8_t dma_inb(uint16_t port, void *priv) { (void)priv; return DmaRead(port); }
static void dma_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaWrite(port,val); }
static uint8_t dmapage_inb(uint16_t port, void *priv) { (void)priv; return DmaRead(port); }
static void dmapage_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaPageWrite(port,val); }

void DmaReset(void)
{
    DmaTemp = 0;
    DmaMode = 0;
    DmaMask = 0;
    DmaClear = 0;
    DmaFlipFlop = FALSE;
    for (int i = 0; i < 4; i++) {
        DmaAddr[i] = 0;
        DmaCount[i] = 0;
    }
    for (int i = 0; i < 16; i++)
        DmaPage[i] = 0;
}

void DmaInit(void)
{
    DmaReset();
    io_sethandler(0x0000, 0x0010, dma_inb, NULL, NULL, dma_outb, NULL, NULL, NULL);
    io_sethandler(0x0080, 0x0010, dmapage_inb, NULL, NULL, dmapage_outb, NULL, NULL, NULL);
}

void DmaWrite(USHORT port, UCHAR val)
{
    if (port <= 0x0007)
    {
        int chan = (port >> 1) & 3;
        if (port & 1)
            DmaCount[chan] = DmaFlipFlop ? (DmaCount[chan] & 0x00FF) | ((USHORT)val << 8) : (DmaCount[chan] & 0xFF00) | val;
        else
            DmaAddr[chan] = DmaFlipFlop ? (DmaAddr[chan] & 0x00FF) | ((USHORT)val << 8) : (DmaAddr[chan] & 0xFF00) | val;
        DmaFlipFlop = !DmaFlipFlop;
    }
    else
    {
        switch(port)
        {
        case 0x000A:
            DmaMask = val;
            break;
        case 0x000B:
            DmaMode = val;
            break;
        case 0x000C:
            DmaFlipFlop = FALSE;
            DmaClear = val;
            break;
        case 0x000D:
            DmaTemp = val;
            break;
        case 0x000F:
            DmaMask = val;
            break;
        default:
            break;
        }
    }
}

void DmaPageWrite(USHORT port, UCHAR val)
{
    DmaPage[port & 0xF] = val;
}

UCHAR DmaRead(USHORT port)
{
    if (port <= 0x0007)
    {
        int chan = (port >> 1) & 3;
        USHORT val = (port & 1) ? DmaCount[chan] : DmaAddr[chan];
        UCHAR b = DmaFlipFlop ? (val >> 8) : (val & 0xFF);
        DmaFlipFlop = !DmaFlipFlop;
        return b;
    }
    else if (port == 0x0008)
    {
        return 0;
    }
    else if (port >= 0x0080 && port <= 0x008F)
    {
        return DmaPage[port & 0xF];
    }
    return 0;
}
