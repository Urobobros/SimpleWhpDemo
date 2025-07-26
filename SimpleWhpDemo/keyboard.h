#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void KeyboardWrite(USHORT port, UCHAR val);
UCHAR KeyboardReadData(void);
UCHAR KeyboardReadStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_H */
