#include "distSense.h"

#define TAG "GP2Y_SENSOR"

DistansceSensor::DistansceSensor(adc_unit_t unit,
                                 adc_channel_t channel,
                                 adc_atten_t atten,
                                 uint8_t samples,
                                 lut_cfg_t lut)
: handle(nullptr), distance(0), index(0), filled(false)
{
    gp2y0a21_cfg_t cfg = {
        .unit = unit,
        .channel = channel,
        .atten = atten,
        .samples_per_read = samples,
        .iir_alpha = 0.2f,
        .enable_calib = true
    };

    esp_err_t err = gp2y0a21_create(&cfg, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GP2Y0A21 (%s)", esp_err_to_name(err));
        return;
    }

    if (lut && lut->mv_points && lut->cm_points && lut->lut_size >= 2) {
        err = gp2y0a21_set_lut(handle, lut->mv_points, lut->cm_points, lut->lut_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Custom LUT applied with %d points", (int)lut->lut_size);
        } else {
            ESP_LOGW(TAG, "Failed to set LUT, using default");
        }
    }
}

DistansceSensor::~DistansceSensor()
{
    if (handle) {
        gp2y0a21_destroy(handle);
    }
}

uint32_t DistansceSensor::readDistance()
{
    if (!handle) return 0;
    float cm = 0.0f;
    if (gp2y0a21_read_distance_cm(handle, &cm) == ESP_OK) {
        distance = (uint32_t)cm;
        return distance;
    }
    return 0;
}

int DistansceSensor::readVoltage()
{
    if (!handle) return -1;
    int mv = 0;
    if (gp2y0a21_read_mv(handle, &mv) == ESP_OK) {
        return mv;
    }
    return -1;
}

uint32_t DistansceSensor::readDistanceFiltered()
{
    uint32_t d = readDistance();
    buffer[index] = d;
    index = (index + 1) % FILTER_SIZE;
    if (index == 0) filled = true;

    int count = filled ? FILTER_SIZE : index;
    uint32_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += buffer[i];
    }
    return (count > 0) ? (sum / count) : d;
}
