#ifndef PIC_H
#define PIC_H
#include <windows.h>
extern UCHAR PicMasterImr;
extern UCHAR PicSlaveImr;
void PicWrite(USHORT port, UCHAR val);
#endif
