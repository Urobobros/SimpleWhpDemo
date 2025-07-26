#ifndef CGA_H
#define CGA_H
#include <windows.h>
extern UCHAR CgaMode;
extern UCHAR AttrCga;
extern UCHAR CgaStatus;
extern ULONGLONG CgaLastToggleMs;
void CgaOut(USHORT port, UCHAR val);
#endif
