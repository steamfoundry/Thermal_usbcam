#include "config.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "thermal_pipeline.h"
#include "status_screens.h"
extern "C" {
#include "MLX90640_I2C_Driver.h"
}
static volatile bool busy=false,live=false;static volatile uint8_t usb_frame_index=0xff;
extern "C" void tud_video_frame_xfer_complete_cb(uint_fast8_t c,uint_fast8_t s){(void)c;(void)s;if(live&&usb_frame_index<2)thermal_release_frame(usb_frame_index);usb_frame_index=0xff;live=false;busy=false;}
extern "C" int tud_video_commit_cb(uint_fast8_t c,uint_fast8_t s,video_probe_and_commit_control_t const*p){(void)c;(void)s;(void)p;return VIDEO_ERROR_NONE;}
int main(){board_init();MLX90640_I2CInit();thermal_pipeline_init();status_screens_init();tusb_init();multicore_launch_core1(thermal_core1_entry);for(;;){tud_task();if(busy||!tud_video_n_streaming(0,0))continue;const screen_mode_t m=status_screen_get_mode();const uint8_t*p=nullptr;uint8_t i=0xff;bool is_live=false;if(m==SCREEN_LIVE){p=thermal_acquire_ready_frame(&i);is_live=p!=nullptr;}else p=status_screen_frame(m);if(!p)continue;usb_frame_index=i;live=is_live;busy=true;if(!tud_video_n_frame_xfer(0,0,(void*)p,UVC_FRAME_BYTES)){busy=false;live=false;usb_frame_index=0xff;if(is_live)thermal_release_frame(i);}}}
