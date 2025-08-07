#ifndef STUBS_H
#define STUBS_H
#include <stdint.h>
void video_updatetiming(void);
uint32_t cpu_get_speed(void);
void device_speed_changed(void);
void picint(int irq);
void picintc(int irq);
void dma_channel_read(int channel);
extern int gated, speakval, speakon;
extern int ppispeakon;
#endif
