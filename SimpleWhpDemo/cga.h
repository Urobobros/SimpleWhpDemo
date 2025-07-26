#ifndef CGA_H
#define CGA_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR CgaMode;
extern UCHAR AttrCga;
extern UCHAR CgaStatus;
extern ULONGLONG CgaLastToggleMs;
void CgaOut(USHORT port, UCHAR val);

#ifdef __cplusplus
}
#endif

#endif /* CGA_H */
