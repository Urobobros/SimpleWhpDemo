#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t count;
    uint16_t reload;
    uint8_t mode;
    uint8_t access;
    uint8_t bcd;
    uint8_t latched;
    uint16_t latch;
    uint8_t rw_low;
} PIT_CHANNEL;

typedef struct {
    PIT_CHANNEL ch[3];
    uint8_t control;
} PIT;
extern PIT pit, pit2;

uint8_t pit_read(uint16_t port, void *priv);
void pit_write(uint16_t port, uint8_t val, void *priv);
void pit_init(void);
void pit_ps2_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PIT_H */
