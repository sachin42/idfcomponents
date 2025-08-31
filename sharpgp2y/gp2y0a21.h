#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_adc/adc_oneshot.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    adc_unit_t          unit;          // ADC_UNIT_1 (recommended)
    adc_channel_t       channel;       // e.g., ADC_CHANNEL_6 for GPIO34 (ESP32)
    adc_atten_t         atten;         // ADC_ATTEN_DB_11
    uint8_t            samples_per_read; // e.g., 15
    float               iir_alpha;     // 0.2f typical
    bool                enable_calib;  // true: use ADC calibration
} gp2y0a21_cfg_t;

typedef struct gp2y0a21_handle_s* gp2y0a21_handle_t;

/** Create + init */
esp_err_t gp2y0a21_create(const gp2y0a21_cfg_t* cfg, gp2y0a21_handle_t* out);

/** Destroy */
void gp2y0a21_destroy(gp2y0a21_handle_t h);

/** Read filtered millivolts */
esp_err_t gp2y0a21_read_mv(gp2y0a21_handle_t h, int* out_mv);

/** Read distance in centimeters (10..80). Returns ESP_OK; clamps to range. */
esp_err_t gp2y0a21_read_distance_cm(gp2y0a21_handle_t h, float* out_cm);

/** Optional: set alpha (0..1). Higher alpha = faster but noisier. */
void gp2y0a21_set_iir_alpha(gp2y0a21_handle_t h, float alpha);

/** Optional: override LUT (pairs of {mV, cm}, length >= 2). Copy is taken. */
esp_err_t gp2y0a21_set_lut(gp2y0a21_handle_t h, const int16_t* mv_points,
                           const uint8_t* cm_points, size_t count);

#ifdef __cplusplus
}
#endif
