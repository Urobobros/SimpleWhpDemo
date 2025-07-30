#ifndef PIC_H
#define PIC_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UCHAR PicMasterImr;
extern UCHAR PicSlaveImr;
void PicInit(void);

#ifdef __cplusplus
}
#endif

#endif /* PIC_H */
