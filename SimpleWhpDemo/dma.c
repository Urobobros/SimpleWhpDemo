#include "dma.h"

UCHAR DmaTemp = 0;
UCHAR DmaMode = 0;
UCHAR DmaMask = 0;
UCHAR DmaClear = 0;
UCHAR DmaPage1 = 0;
UCHAR DmaPage3 = 0;
USHORT DmaAddr[4] = {0};
USHORT DmaCount[4] = {0};
BOOL DmaFlipFlop = FALSE;

#include "io.h"
#include <stddef.h>

static uint8_t dma_inb(uint16_t port, void *priv) { (void)priv; return DmaRead(port); }
static void dma_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaWrite(port,val); }
static uint8_t dmapage_inb(uint16_t port, void *priv) { (void)priv; return DmaRead(port); }
static void dmapage_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaPageWrite(port,val); }

void DmaInit(void)
{
    io_sethandler(0x0000, 0x0010, dma_inb, NULL, NULL, dma_outb, NULL, NULL, NULL);
    io_sethandler(0x0080, 0x0008, dmapage_inb, NULL, NULL, dmapage_outb, NULL, NULL, NULL);
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
        default:
            break;
        }
    }
}

void DmaPageWrite(USHORT port, UCHAR val)
{
    if (port == 0x0081)
        DmaPage1 = val;
    else if (port == 0x0083)
        DmaPage3 = val;  
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
    else if (port == 0x0081)
    {
        return DmaPage1;
    }
    else if (port == 0x0083)
    {
        return DmaPage3;
    }
    return 0;
}
