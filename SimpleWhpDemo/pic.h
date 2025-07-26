#ifndef PIC_H
#define PIC_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR PicMasterImr;
extern UCHAR PicSlaveImr;
void PicWrite(USHORT port, UCHAR val);

#ifdef __cplusplus
}
#endif

#endif /* PIC_H */
