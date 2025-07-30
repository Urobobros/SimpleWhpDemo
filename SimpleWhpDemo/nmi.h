#ifndef NMI_H
#define NMI_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR NmiMask;
void NmiWrite(USHORT port, UCHAR val);
void NmiInit(void);

#ifdef __cplusplus
}
#endif

#endif /* NMI_H */
