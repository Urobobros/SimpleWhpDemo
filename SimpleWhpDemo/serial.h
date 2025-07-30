#ifndef SERIAL_H
#define SERIAL_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void SerialInit(void);
void serial1_init(USHORT addr, int irq, int has_fifo);
void serial2_init(USHORT addr, int irq, int has_fifo);
void SerialWrite(USHORT port, UCHAR val);
UCHAR SerialRead(USHORT port);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
