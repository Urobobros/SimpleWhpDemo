#ifndef NMI_H
#define NMI_H
#include <windows.h>
extern UCHAR NmiMask;
void NmiWrite(USHORT port, UCHAR val);
#endif
