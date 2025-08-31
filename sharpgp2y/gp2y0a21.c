#include "gp2y0a21.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdlib.h>
#include <string.h>

#define TAG "GP2Y0A21"

// ---------- Typical LUT from datasheet curve (rounded) ----------
// Voltage(mV)  -> Distance(cm). Higher V => shorter distance.
static const int16_t s_def_mv[]  = {3000, 2800, 2400, 2000, 1600, 1200, 1000,  850,  700,  600,  500,  450};
static const uint8_t s_def_cm[]  = {  10,   12,   15,   20,   25,   30,   35,   40,   50,   60,   70,   80};
static const size_t  s_def_n     = sizeof(s_def_cm)/sizeof(s_def_cm[0]);

typedef struct gp2y0a21_handle_s {
    adc_oneshot_unit_handle_t adc;
    adc_cali_handle_t         cali;
    bool                      have_cali;
    adc_channel_t             channel;
    uint32_t                  samples;
    float                     alpha;
    int                       last_mv; // smoothed mV (IIR)
    // LUT (modifiable)
    int16_t* mv;
    uint8_t* cm;
    size_t   n;
} gp2y0a21_t;

static int cmp_int16(const void* a, const void* b) {
    int16_t va = *(const int16_t*)a, vb = *(const int16_t*)b;
    return (va>vb) - (va<vb);
}

static int median_of(int16_t* arr, size_t n) {
    qsort(arr, n, sizeof(arr[0]), cmp_int16);
    if (n & 1) return arr[n/2];
    return (arr[n/2 - 1] + arr[n/2]) / 2;
}

static float interp_cm_from_mv(const gp2y0a21_t* h, int mv) {
    // Clamp outside LUT
    if (mv >= h->mv[0])           return h->cm[0];
    if (mv <= h->mv[h->n - 1])    return h->cm[h->n - 1];

    // Find segment [i, i+1] with mv in (mv[i+1], mv[i]]
    for (size_t i = 0; i + 1 < h->n; ++i) {
        int mv_hi = h->mv[i];
        int mv_lo = h->mv[i+1];
        if (mv <= mv_hi && mv >= mv_lo) {
            int d_mv = mv_hi - mv_lo;
            float t = d_mv ? (float)(mv - mv_lo) / (float)d_mv : 0.0f; // 0..1
            float cm_lo = (float)h->cm[i+1];
            float cm_hi = (float)h->cm[i];
            // mv decreases -> cm increases, so invert t
            float cm = cm_lo + (1.0f - t) * (cm_hi - cm_lo);
            if (cm < 10.0f) cm = 10.0f;
            if (cm > 80.0f) cm = 80.0f;
            return cm;
        }
    }
    // Fallback (shouldn't happen)
    return (float)h->cm[h->n - 1];
}

esp_err_t gp2y0a21_create(const gp2y0a21_cfg_t* cfg, gp2y0a21_handle_t* out) {
    if (!cfg || !out) return ESP_ERR_INVALID_ARG;
    *out = NULL;

    gp2y0a21_t* h = calloc(1, sizeof(*h));
    if (!h) return ESP_ERR_NO_MEM;

    // ADC oneshot init
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = cfg->unit,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &h->adc), TAG, "adc unit");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = cfg->atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(h->adc, cfg->channel, &chan_cfg), TAG, "adc ch");

    // Calibration (if available)
    h->have_cali = false;
    h->cali = NULL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (cfg->enable_calib) {
        adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = cfg->unit,
            .chan = cfg->channel,
            .atten = cfg->atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT
        };
        if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &h->cali) == ESP_OK) {
            h->have_cali = true;
            ESP_LOGI(TAG, "ADC calibration: curve fitting");
        } else {
            ESP_LOGW(TAG, "ADC calibration not available, using raw");
        }
    }
#else
    (void)cfg;
#endif

    h->channel = cfg->channel;
    h->samples = (cfg->samples_per_read > 0) ? cfg->samples_per_read : 15;
    h->alpha   = (cfg->iir_alpha >= 0.0f && cfg->iir_alpha <= 1.0f) ? cfg->iir_alpha : 0.2f;
    h->last_mv = -1;

    // Copy default LUT
    h->n  = s_def_n;
    h->mv = malloc(h->n * sizeof(int16_t));
    h->cm = malloc(h->n * sizeof(uint8_t));
    if (!h->mv || !h->cm) {
        gp2y0a21_destroy(h);
        return ESP_ERR_NO_MEM;
    }
    memcpy(h->mv, s_def_mv, sizeof(s_def_mv));
    memcpy(h->cm, s_def_cm, sizeof(s_def_cm));

    *out = h;
    return ESP_OK;
}

void gp2y0a21_destroy(gp2y0a21_handle_t h_) {
    if (!h_) return;
    gp2y0a21_t* h = (gp2y0a21_t*)h_;
    if (h->cali && h->have_cali) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(h->cali);
#endif
    }
    if (h->adc) adc_oneshot_del_unit(h->adc);
    free(h->mv);
    free(h->cm);
    free(h);
}

void gp2y0a21_set_iir_alpha(gp2y0a21_handle_t h_, float alpha) {
    gp2y0a21_t* h = (gp2y0a21_t*)h_;
    if (!h) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    h->alpha = alpha;
}

esp_err_t gp2y0a21_set_lut(gp2y0a21_handle_t h_, const int16_t* mv_points,
                           const uint8_t* cm_points, size_t count) {
    gp2y0a21_t* h = (gp2y0a21_t*)h_;
    if (!h || !mv_points || !cm_points || count < 2) return ESP_ERR_INVALID_ARG;

    int16_t* mv_new = malloc(count * sizeof(int16_t));
    uint8_t* cm_new = malloc(count * sizeof(uint8_t));
    if (!mv_new || !cm_new) { free(mv_new); free(cm_new); return ESP_ERR_NO_MEM; }

    memcpy(mv_new, mv_points, count*sizeof(int16_t));
    memcpy(cm_new, cm_points, count*sizeof(uint8_t));

    // Ensure strictly descending mV for interpolation logic
    for (size_t i = 1; i < count; ++i) {
        if (!(mv_new[i-1] > mv_new[i])) {
            // If unordered, just sort both arrays by mv descending, keeping pairs
            // Simple bubble; count is tiny
            for (size_t p = 0; p < count; ++p) {
                for (size_t q = p+1; q < count; ++q) {
                    if (mv_new[p] < mv_new[q]) {
                        int16_t tmv = mv_new[p]; mv_new[p] = mv_new[q]; mv_new[q] = tmv;
                        uint8_t tcm = cm_new[p]; cm_new[p] = cm_new[q]; cm_new[q] = tcm;
                    }
                }
            }
            break;
        }
    }

    free(h->mv); free(h->cm);
    h->mv = mv_new; h->cm = cm_new; h->n = count;
    return ESP_OK;
}

static esp_err_t read_single_mv(gp2y0a21_t* h, int* out_mv) {
    int raw = 0;
    esp_err_t err = adc_oneshot_read(h->adc, h->channel, &raw);
    if (err != ESP_OK) return err;

    int mv = 0;
    if (h->have_cali) {
        err = adc_cali_raw_to_voltage(h->cali, raw, &mv);
        if (err != ESP_OK) return err;
    } else {
        // Fallback rough scaling for 12-bit at 11dB: ~0..3300 mV
        // This is approximate; calibration strongly recommended.
        mv = (raw * 3300) / 4095;
    }
    *out_mv = mv;
    return ESP_OK;
}

esp_err_t gp2y0a21_read_mv(gp2y0a21_handle_t h_, int* out_mv) {
    gp2y0a21_t* h = (gp2y0a21_t*)h_;
    if (!h || !out_mv) return ESP_ERR_INVALID_ARG;

    uint8_t n = h->samples;
    if (n < 3) n = 3;
    int16_t* buf = (int16_t*)alloca(n * sizeof(int16_t));
    for (uint8_t i = 0; i < n; ++i) {
        int mv = 0;
        esp_err_t e = read_single_mv(h, &mv);
        if (e != ESP_OK) return e;
        buf[i] = (int16_t)mv;
    }

    int med = median_of(buf, n);

    if (h->last_mv < 0) {
        h->last_mv = med; // first-time prime
    } else {
        float f = h->alpha * (float)med + (1.0f - h->alpha) * (float)h->last_mv;
        h->last_mv = (int)(f + 0.5f);
    }

    *out_mv = h->last_mv;
    return ESP_OK;
}

esp_err_t gp2y0a21_read_distance_cm(gp2y0a21_handle_t h_, float* out_cm) {
    gp2y0a21_t* h = (gp2y0a21_t*)h_;
    if (!h || !out_cm) return ESP_ERR_INVALID_ARG;

    int mv = 0;
    ESP_RETURN_ON_ERROR(gp2y0a21_read_mv(h, &mv), TAG, "read mv");
    float cm = interp_cm_from_mv(h, mv);
    // Final clamp for safety
    if (cm < 10.0f) cm = 10.0f;
    if (cm > 80.0f) cm = 80.0f;
    *out_cm = cm;
    return ESP_OK;
}
