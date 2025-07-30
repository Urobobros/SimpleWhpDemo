#ifndef PIC_H
#define PIC_H

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR PicMasterImr;
extern UCHAR PicSlaveImr;
void PicInit(void);
UCHAR PicRead(USHORT port, void *priv);
void PicWrite(USHORT port, UCHAR val, void *priv);

#ifdef __cplusplus
}
#endif

#endif /* PIC_H */
