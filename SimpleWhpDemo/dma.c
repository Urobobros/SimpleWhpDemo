#include "dma.h"
#include <stdint.h>

UCHAR DmaTemp = 0;
UCHAR DmaMode = 0;
UCHAR DmaMask = 0;
UCHAR DmaClear = 0;
UCHAR DmaCommand = 0;
UCHAR DmaStatus = 0;
UCHAR DmaPage[16] = {0};
#define DmaPage1 (DmaPage[1])
#define DmaPage3 (DmaPage[3])
USHORT DmaAddr[4] = {0};
USHORT DmaCount[4] = {0};
BOOL DmaFlipFlop = FALSE;

static uint8_t dmaregs[16];
static uint8_t dmapages[16];

/*DMA*/
typedef struct dma_t {
        uint32_t ab, ac;
        uint16_t cb;
        int cc;
        int wp;
        uint8_t m, mode;
        uint8_t page;
        uint8_t stat, stat_rq;
        uint8_t command;
        int size;

        uint8_t ps2_mode;
        uint8_t arb_level;
        uint16_t io_addr;
} dma_t;

static int dma_wp;
dma_t dma[8];
static uint8_t dma_m;
static uint8_t dma_stat;
static uint8_t dma_command;



#include "io.h"
#include <stddef.h>
#include <stdio.h>

uint8_t dma_page_read(USHORT addr);
void DmaPageWrite(USHORT addr, UCHAR val);

static uint8_t dma_inb(uint16_t port, void *priv) { (void)priv; return DmaRead(port); }
static void dma_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaWrite(port,val); }
static uint8_t dma_page_inb(uint16_t port, void *priv) { (void)priv; return dma_page_read(port); }
static void dma_page_outb(uint16_t port, uint8_t val, void *priv) { (void)priv; DmaPageWrite(port,val); }

uint8_t dma_page_read(USHORT addr) { 
    uint8_t returnData = dmapages[addr & 0xf];
    printf("dma_page_read %04X %02X \n",addr,returnData);
    return returnData; 
}

void DmaPageWrite(USHORT addr, UCHAR val) {
        printf("dma_page_write %04X %02X \n",addr,val);

        dmapages[addr & 0xf] = val;
        switch (addr & 0xf) {
        case 1:
                dma[2].page = val & 0xf;
                dma[2].ab = (dma[2].ab & 0xffff) | (dma[2].page << 16);
                dma[2].ac = (dma[2].ac & 0xffff) | (dma[2].page << 16);
                break;
        case 2:
                dma[3].page = val & 0xf;
                dma[3].ab = (dma[3].ab & 0xffff) | (dma[3].page << 16);
                dma[3].ac = (dma[3].ac & 0xffff) | (dma[3].page << 16);
                break;
        case 3:
                dma[1].page = val & 0xf;
                dma[1].ab = (dma[1].ab & 0xffff) | (dma[1].page << 16);
                dma[1].ac = (dma[1].ac & 0xffff) | (dma[1].page << 16);
                break;
        case 7:
                dma[0].page = val & 0xf;
                dma[0].ab = (dma[0].ab & 0xffff) | (dma[0].page << 16);
                dma[0].ac = (dma[0].ac & 0xffff) | (dma[0].page << 16);
                break;
        case 0x9:
                dma[6].page = val & 0xfe;
                dma[6].ab = (dma[6].ab & 0x1ffff) | (dma[6].page << 16);
                dma[6].ac = (dma[6].ac & 0x1ffff) | (dma[6].page << 16);
                break;
        case 0xa:
                dma[7].page = val & 0xfe;
                dma[7].ab = (dma[7].ab & 0x1ffff) | (dma[7].page << 16);
                dma[7].ac = (dma[7].ac & 0x1ffff) | (dma[7].page << 16);
                break;
        case 0xb:
                dma[5].page = val & 0xfe;
                dma[5].ab = (dma[5].ab & 0x1ffff) | (dma[5].page << 16);
                dma[5].ac = (dma[5].ac & 0x1ffff) | (dma[5].page << 16);
                break;
        }
}


// void DmaReset(void)
// {
//     DmaTemp = 0;
//     DmaMode = 0;
//     DmaMask = 0;
//     DmaClear = 0;
//     DmaCommand = 0;
//     DmaStatus = 0;
//     DmaFlipFlop = FALSE;
//     for (int i = 0; i < 4; i++) {
//         DmaAddr[i] = 0;
//         DmaCount[i] = 0;
//     }
//     for (int i = 0; i < 16; i++)
//         DmaPage[i] = 0;
// }


void DmaReset() {
        int c;

        dma_wp = 0;
        dma_m = 0;

        for (c = 0; c < 16; c++)
                dmaregs[c] = 0;
        for (c = 0; c < 8; c++) {
                dma[c].mode = 0;
                dma[c].ac = 0;
                dma[c].cc = 0;
                dma[c].ab = 0;
                dma[c].cb = 0;
                dma[c].size = (c & 4) ? 1 : 0;
        }
}

void DmaInit(void)
{
    DmaReset();
    io_sethandler(0x0000, 0x0010, dma_inb, NULL, NULL, dma_outb, NULL, NULL, NULL);
    /* Only the first eight page registers are used */
    io_sethandler(0x0080, 0x0008, dma_page_inb, NULL, NULL, dma_page_outb, NULL, NULL, NULL);
}

void DmaWrite(USHORT port, uint8_t val) {
        int channel = (port >> 1) & 3;
                printf("Write DMA %04X %02X \n",port,val);
        dmaregs[port & 0xf] = val;
        switch (port) {
        case 0x0000:
        case 0x0002:
        case 0x0004:
        case 0x0006: /*Address registers*/
                dma_wp ^= 1;
                if (dma_wp)
                        dma[channel].ab = (dma[channel].ab & 0xffff00) | val;
                else
                        dma[channel].ab = (dma[channel].ab & 0xff00ff) | (val << 8);
                dma[channel].ac = dma[channel].ab;
                //                pclog("Addr = %04x\n", dma.ab[(addr >> 1) & 3]);
                return;

        case 0x0001:
        case 0x0003:
        case 0x0005:
        case 0x0007: /*Count registers*/
                dma_wp ^= 1;
                if (dma_wp)
                        dma[channel].cb = (dma[channel].cb & 0xff00) | val;
                else
                        dma[channel].cb = (dma[channel].cb & 0x00ff) | (val << 8);
                dma[channel].cc = dma[channel].cb;
                return;

        case 0x0008: /*Control register*/
                dma_command = val;
                return;

        case 0x000a: /*Mask*/
                if (val & 4)
                        dma_m |= (1 << (val & 3));
                else
                        dma_m &= ~(1 << (val & 3));
                return;

        case 0x000b: /*Mode*/
                channel = (val & 3);
                dma[channel].mode = val;
                // if (dma_ps2.is_ps2) {
                //         dma[channel].ps2_mode &= ~0x1c;
                //         if (val & 0x20)
                //                 dma[channel].ps2_mode |= 0x10;
                //         if ((val & 0xc) == 8)
                //                 dma[channel].ps2_mode |= 4;
                //         else if ((val & 0xc) == 4)
                //                 dma[channel].ps2_mode |= 0xc;
                // }
                return;

        case 0x000c: /*Clear FF*/
                dma_wp = 0;
                return;

        case 0x000d: /*Master clear*/
                dma_wp = 0;
                dma_m |= 0xf;
                return;

        case 0x000f: /*Mask write*/
                dma_m = (dma_m & 0xf0) | (val & 0xf);
                return;
        }
}


// void DmaWrite(USHORT port, UCHAR val)
// {
//     if (port <= 0x0007)
//     {
//         int chan = (port >> 1) & 3;
//         if (port & 1)
//             DmaCount[chan] = DmaFlipFlop ? (DmaCount[chan] & 0x00FF) | ((USHORT)val << 8) : (DmaCount[chan] & 0xFF00) | val;
//         else
//             DmaAddr[chan] = DmaFlipFlop ? (DmaAddr[chan] & 0x00FF) | ((USHORT)val << 8) : (DmaAddr[chan] & 0xFF00) | val;
//         DmaFlipFlop = !DmaFlipFlop;
//     }
//     else
//     {
//         switch(port)
//         {
//         case 0x0008:
//             DmaCommand = val;
//             break;
//         case 0x000A:
//             DmaMask = val;
//             break;
//         case 0x000B:
//             DmaMode = val;
//             break;
//         case 0x000C:
//             DmaFlipFlop = FALSE;
//             DmaClear = val;
//             break;
//         case 0x000D:
//             DmaTemp = val;
//             break;
//         case 0x000F:
//             DmaMask = val;
//             break;
//         default:
//             break;
//         }
//     }
// }

// void DmaPageWrite(USHORT port, UCHAR val)
// {
//     DmaPage[port & 0xF] = val;
// }


uint8_t DmaRead(USHORT port) {
        uint8_t returnData = 0; 
        uint8_t temp;
        int channel = (port >> 1) & 3;

        switch (port) {
            case 0x0000:
            case 0x0002:
            case 0x0004:
            case 0x0006: /*Address registers*/

                    dma_wp ^= 1;
                    if (dma_wp)
                            returnData = dma[channel].ac & 0xff;
                    else
                        returnData = (dma[channel].ac >> 8) & 0xff;
                    break;

            case 0x0001:
            case 0x0003:
            case 0x0005:
            case 0x0007: /*Count registers*/
                    dma_wp ^= 1;
                    if (dma_wp)
                            returnData = dma[channel].cc & 0xff;
                    else
                            returnData = dma[channel].cc >> 8;
                    break;        

            case 0x0008: /*Status register*/
                    temp = dma_stat & 0xf;
                    dma_stat &= ~0xf;
                    returnData = temp;
                    break;

            case 0x000d:
                    returnData = 0;
                    break;

            default: 
                    printf("Bad DMA read %04X \n",port);
                    returnData = dmaregs[port & 0xf];
                    break;
        }
        
        printf("Read DMA %04X %02X \n",port, returnData);

        return returnData;            
}


//UCHAR DmaRead(USHORT port)
//{
//     if (port <= 0x0007)
//     {
//         int chan = (port >> 1) & 3;
//         USHORT val = (port & 1) ? DmaCount[chan] : DmaAddr[chan];
//         UCHAR b = DmaFlipFlop ? (val >> 8) : (val & 0xFF);
//         DmaFlipFlop = !DmaFlipFlop;
//         return b;
//     }
//     else if (port == 0x0008)
//     {
//         UCHAR val = DmaStatus;
//         DmaStatus = 0;
//         return val;
//     }
//     else if (port >= 0x0080 && port <= 0x008F)
//     {
//         return DmaPage[port & 0xF];
//     }
//     return 0;
// }
