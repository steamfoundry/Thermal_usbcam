#pragma once
#include <stdint.h>
typedef enum { SCREEN_LOADING=0, SCREEN_NO_SENSOR, SCREEN_RANGE_ERROR, SCREEN_LIVE } screen_mode_t;
#ifdef __cplusplus
extern "C" {
#endif
void status_screens_init(void);
void status_screen_set_mode(screen_mode_t mode);
screen_mode_t status_screen_get_mode(void);
const uint8_t *status_screen_frame(screen_mode_t mode);
#ifdef __cplusplus
}
#endif
