#include <stdint.h>
#include <stdio.h>

/* Stub implementations required for unit tests */

/* Called when PIT clock or CPU speed changes. No-op for tests. */
void video_updatetiming(void) {}

/* Return nominal CPU speed for XT multiplier calculation. */
uint32_t cpu_get_speed(void) { return 4770000; }

/* Notify other devices that speed changed. */
void device_speed_changed(void) {}

/* Programmable interrupt controller callbacks used by PIT */
void picint(int irq) { (void)irq; }
void picintc(int irq) { (void)irq; }

/* DMA helper used when PIT triggers refresh on channel 0 */
void dma_channel_read(int channel) { (void)channel; }

/* Speaker state used by PIT */
int gated = 0, speakval = 0, speakon = 0;
int ppispeakon = 0;

