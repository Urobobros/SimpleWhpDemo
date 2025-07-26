#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <windows.h>
void KeyboardWrite(USHORT port, UCHAR val);
UCHAR KeyboardReadData(void);
UCHAR KeyboardReadStatus(void);
#endif
