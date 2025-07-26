#ifndef DMA_H
#define DMA_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR DmaTemp;
extern UCHAR DmaMode;
extern UCHAR DmaMask;
extern UCHAR DmaClear;
extern UCHAR DmaPage1;
extern USHORT DmaAddr[4];
extern USHORT DmaCount[4];
extern BOOL DmaFlipFlop;
void DmaWrite(USHORT port, UCHAR val);
void DmaPageWrite(USHORT port, UCHAR val);
UCHAR DmaRead(USHORT port);

#ifdef __cplusplus
}
#endif

#endif /* DMA_H */
