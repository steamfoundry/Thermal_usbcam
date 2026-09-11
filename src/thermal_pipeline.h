#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void thermal_pipeline_init(void);
void thermal_core1_entry(void);
const uint8_t *thermal_acquire_ready_frame(uint8_t *index);
void thermal_release_frame(uint8_t index);
uint32_t thermal_frames_produced(void);
uint32_t thermal_sensor_errors(void);
uint32_t thermal_frames_dropped(void);
#ifdef __cplusplus
}
#endif
