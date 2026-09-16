/**
 * @file thermal_pipeline.cpp
 *
 * Thermal image processing pipeline.
 *
 * Processing Flow:
 *
 *   MLX90640
 *        ↓
 *   Temperature Frame
 *        ↓
 *   Validation
 *        ↓
 *   Dynamic Range Scaling
 *        ↓
 *   Palette Mapping
 *        ↓
 *   Interpolation
 *        ↓
 *   YUY2 Frame
 *        ↓
 *   USB UVC Streaming
 *
 * Buffer State Machine:
 *
 *   0 = Free
 *   1 = Producer Owned
 *   2 = Ready
 *   3 = USB Owned
 *
 * NOTE:
 * Invalid pixels currently use the existing
 * replacement strategy implemented by V3.2.
 *
 * V3.3 Documentation Release
 */

#include "thermal_pipeline.h"
#include "config.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "status_screens.h"
extern "C" {
#include "MLX90640_API.h"
}
#include <math.h>
namespace {
struct Yuv {
  uint8_t y, u, v;
};
alignas(4) uint8_t fb[2][UVC_FRAME_BYTES];
volatile uint8_t st[2] = {};
volatile uint32_t made = 0, errs = 0, drops = 0;
Yuv pal[256];
uint8_t lut[TEMP_LUT_SIZE], x0[UVC_W], x1[UVC_W], xf[UVC_W], y0[UVC_H],
    y1[UVC_H], yf[UVC_H];
paramsMLX90640 prm;
int ci(int v, int l, int h) { return v < l ? l : v > h ? h : v; }
uint8_t c8(int v) { return (uint8_t)ci(v, 0, 255); }
void hsv(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint16_t d = (255 - h) * 240 / 255;
  uint8_t q = 255 - (d % 60) * 255 / 60, t = (d % 60) * 255 / 60;
  switch (d / 60) {
  case 0:
    r = 255;
    g = t;
    b = 0;
    break;
  case 1:
    r = q;
    g = 255;
    b = 0;
    break;
  case 2:
    r = 0;
    g = 255;
    b = t;
    break;
  case 3:
    r = 0;
    g = q;
    b = 255;
    break;
  default:
    r = t;
    g = 0;
    b = 255;
  }
}
void init_tables() {
  for (int i = 0; i < 256; i++) {
    uint8_t r, g, b;
    hsv(i, r, g, b);
    pal[i] = {c8(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16),
              c8(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128),
              c8(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128)};
  }
  for (int x = 0; x < UVC_W; x++) {
    uint32_t q = (uint32_t)x * (THERMAL_W - 1) * 256 / (UVC_W - 1);
    x0[x] = q >> 8;
    x1[x] = x0[x] + 1 < THERMAL_W ? x0[x] + 1 : x0[x];
    xf[x] = q;
  }
  for (int y = 0; y < UVC_H; y++) {
    uint32_t q = (uint32_t)y * (THERMAL_H - 1) * 256 / (UVC_H - 1);
    y0[y] = q >> 8;
    y1[y] = y0[y] + 1 < THERMAL_H ? y0[y] + 1 : y0[y];
    yf[y] = q;
  }
}
void mk_lut(int lo, int hi) {
  if (hi - lo < 10) {
    int m = (hi + lo) / 2;
    lo = m - 5;
    hi = m + 5;
  }
  int span = hi - lo;
  for (int t = MLX_VALID_MIN_T10; t <= MLX_VALID_MAX_T10; t++)
    lut[t - MLX_VALID_MIN_T10] =
        (uint8_t)(t <= lo   ? 0
                  : t >= hi ? 255
                            : ((t - lo) * 255 + span / 2) / span);
}
uint8_t ix(int32_t q) {
  int t = (q * 10 + (q >= 0 ? 128 : -128)) / 256;
  return lut[ci(t, MLX_VALID_MIN_T10, MLX_VALID_MAX_T10) - MLX_VALID_MIN_T10];
}
void pair(uint8_t *d, int p, Yuv a, Yuv b) {
  d += p * 2;
  d[0] = a.y;
  d[1] = (a.u + b.u) / 2;
  d[2] = b.y;
  d[3] = (a.v + b.v) / 2;
}
void render(int32_t *s, uint8_t *d) {
  for (int y = 0; y < UVC_H; y++)
    for (int x = 0; x < UVC_W; x += 2) {
      Yuv c[2];
      for (int k = 0; k < 2; k++) {
        int z = x + k, wx = xf[z], wy = yf[y];
        int64_t a = (int64_t)s[y0[y] * 32 + x0[z]] * (256 - wx) +
                    (int64_t)s[y0[y] * 32 + x1[z]] * wx,
                b = (int64_t)s[y1[y] * 32 + x0[z]] * (256 - wx) +
                    (int64_t)s[y1[y] * 32 + x1[z]] * wx;
        c[k] = pal[ix((a * (256 - wy) + b * wy + 32768) >> 16)];
      }
      pair(d, y * UVC_W + x, c[0], c[1]);
    }
}
/*V3 .4 explicit pixel -
    validity mask.*Temperature and validity are stored separately,
    so a valid 0.0 C reading *is no longer confused with a dead
            pixel.Each word represents one row.*/
bool pixel_valid(const uint32_t valid_rows[THERMAL_H], int index) {
  const unsigned row = (unsigned)index / THERMAL_W;
  const unsigned col = (unsigned)index % THERMAL_W;
  return (valid_rows[row] & ((uint32_t)1u << col)) != 0;
}

void mark_pixel_valid(uint32_t valid_rows[THERMAL_H], int index) {
  const unsigned row = (unsigned)index / THERMAL_W;
  const unsigned col = (unsigned)index % THERMAL_W;
  valid_rows[row] |= (uint32_t)1u << col;
}

// Melexis EEPROM lists contain up to five entries and use 0xFFFF as the
// terminator. Entries outside the native 0..767 range are ignored defensively.
bool pixel_in_bad_list(const uint16_t pixels[5], int index) {
  for (int i = 0; i < 5; ++i) {
    const uint16_t pixel = pixels[i];
    if (pixel == UINT16_MAX)
      break;
    if (pixel < THERMAL_W * THERMAL_H && pixel == (uint16_t)index)
      return true;
  }
  return false;
}

bool pixel_factory_bad(const paramsMLX90640 &params, int index) {
  return pixel_in_bad_list(params.brokenPixels, index) ||
         pixel_in_bad_list(params.outlierPixels, index);
}

int32_t median_values(int32_t *values, int count) {
  for (int i = 1; i < count; ++i) {
    const int32_t value = values[i];
    int j = i;
    while (j > 0 && values[j - 1] > value) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = value;
  }
  if (count & 1)
    return values[count / 2];
  return (int32_t)(((int64_t)values[count / 2 - 1] + values[count / 2]) / 2);
}

int32_t repair_pixel(const int32_t source_q8[THERMAL_W * THERMAL_H],
                     const uint32_t valid_rows[THERMAL_H], int index,
                     int32_t fallback_q8) {
  const int cx = index % THERMAL_W;
  const int cy = index / THERMAL_W;
  for (int radius = 1; radius <= 3; ++radius) {
    int32_t neighbours[24];
    int count = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (dx != -radius && dx != radius && dy != -radius && dy != radius)
          continue;
        const int x = cx + dx;
        const int y = cy + dy;
        if (x < 0 || x >= THERMAL_W || y < 0 || y >= THERMAL_H)
          continue;
        const int candidate = y * THERMAL_W + x;
        if (pixel_valid(valid_rows, candidate))
          neighbours[count++] = source_q8[candidate];
      }
    }
    if (count != 0)
      return median_values(neighbours, count);
  }
  return fallback_q8;
}
int claim() {
  for (int i = 0; i < 2; i++) {
    uint32_t q = save_and_disable_interrupts();
    if (st[i] == 0) {
      st[i] = 1;
      restore_interrupts(q);
      return i;
    }
    restore_interrupts(q);
  }
  return -1;
}
} // namespace
extern "C" void thermal_pipeline_init() { init_tables(); }
extern "C" void thermal_core1_entry() {
  uint16_t ee[832], raw[834];
  float t[768];
  int32_t q8[768];
  absolute_time_t until = make_timeout_time_ms(STARTUP_DELAY_MS);
  while (absolute_time_diff_us(get_absolute_time(), until) > 0)
    sleep_ms(20);
  for (;;) {
    if (MLX90640_DumpEE(MLX_ADDR, ee) || MLX90640_ExtractParameters(ee, &prm) ||
        MLX90640_SetChessMode(MLX_ADDR) ||
        MLX90640_SetRefreshRate(MLX_ADDR, 4)) {
      errs++;
      status_screen_set_mode(SCREEN_NO_SENSOR);
      sleep_ms(1000);
      continue;
    }
    for (;;) {
      bool seen[2] = {};
      bool ok = true;
      for (int n = 0; n < 6 && !(seen[0] && seen[1]); n++) {
        if (MLX90640_GetFrameData(MLX_ADDR, raw) < 0) {
          ok = false;
          break;
        }
        int sp = MLX90640_GetSubPageNumber(raw);
        if (sp < 0 || sp > 1) {
          ok = false;
          break;
        }
        float ta = MLX90640_GetTa(raw, &prm);
        MLX90640_CalculateTo(raw, &prm, EMISSIVITY, ta - 8, t);
        seen[sp] = true;
      }
      if (!ok || !seen[0] || !seen[1]) {
        errs++;
        status_screen_set_mode(SCREEN_NO_SENSOR);
        sleep_ms(250);
        break;
      }
      /* Old dead pixel code
      int lo = MLX_VALID_MAX_T10, hi = MLX_VALID_MIN_T10, valid = 0;
      for (int i = 0; i < 768; i++) {
        int v = isfinite(t[i]) ? (int)lrintf(t[i] * 10) : MLX_VALID_MAX_T10 + 1;
        if (v < MLX_VALID_MIN_T10 || v > MLX_VALID_MAX_T10) {
          q8[i] = 0;
          continue;
        }
        q8[i] = (int32_t)lrintf(t[i] * 256);
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
        valid++;
      }
      if (valid < 576) {
        errs++;
        status_screen_set_mode(SCREEN_RANGE_ERROR);
        continue;
      }
      for (int i = 0; i < 768; i++)
        if (q8[i] == 0)
          q8[i] = lo * 256 / 10;
      mk_lut(lo, hi);
      */
      // Replaced code
      int lo = MLX_VALID_MAX_T10, hi = MLX_VALID_MIN_T10, valid = 0;
      uint32_t valid_rows[THERMAL_H] = {};
      for (int i = 0; i < THERMAL_W * THERMAL_H; i++) {
        int v = isfinite(t[i]) ? (int)lrintf(t[i] * 10) : MLX_VALID_MAX_T10 + 1;
        if (v < MLX_VALID_MIN_T10 || v > MLX_VALID_MAX_T10 ||
            pixel_factory_bad(prm, i))
          continue;
        q8[i] = (int32_t)lrintf(t[i] * 256);
        mark_pixel_valid(valid_rows, i);
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
        valid++;
      }
      if (valid < (THERMAL_W * THERMAL_H * 3) / 4) {
        errs++;
        status_screen_set_mode(SCREEN_RANGE_ERROR);
        continue;
      }
      const int32_t fallback_q8 = (int32_t)(((int64_t)(lo + hi) * 256) / 20);
      for (int i = 0; i < THERMAL_W * THERMAL_H; i++)
        if (!pixel_valid(valid_rows, i))
          q8[i] = repair_pixel(q8, valid_rows, i, fallback_q8);
      mk_lut(lo, hi);
      int n = claim();
      if (n < 0) {
        drops++;
        while ((n = claim()) < 0)
          __wfe();
      }
      render(q8, fb[n]);
      __dmb();
      st[n] = 2;
      made++;
      status_screen_set_mode(SCREEN_LIVE);
      __sev();
    }
  }
}
extern "C" const uint8_t *thermal_acquire_ready_frame(uint8_t *i) {
  if (!i)
    return nullptr;
  for (int n = 0; n < 2; n++) {
    uint32_t q = save_and_disable_interrupts();
    if (st[n] == 2) {
      st[n] = 3;
      restore_interrupts(q);
      *i = n;
      return fb[n];
    }
    restore_interrupts(q);
  }
  return nullptr;
}
extern "C" void thermal_release_frame(uint8_t i) {
  if (i < 2) {
    __dmb();
    st[i] = 0;
    __sev();
  }
}
extern "C" uint32_t thermal_frames_produced() { return made; }
extern "C" uint32_t thermal_sensor_errors() { return errs; }
extern "C" uint32_t thermal_frames_dropped() { return drops; }
