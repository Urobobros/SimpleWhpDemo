#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void KeyboardWrite(USHORT port, UCHAR val);
UCHAR KeyboardReadData(void);
UCHAR KeyboardReadStatus(void);
UCHAR KeyboardXtRead(USHORT port);
void KeyboardXtWrite(USHORT port, UCHAR val);
void KeyboardInit(void);

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_H */

