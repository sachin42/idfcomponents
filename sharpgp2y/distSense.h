#pragma once

#include "esp_log.h"
#include "gp2y0a21.h"

struct lut_cfg
{
    const int16_t *mv_points;
    const uint8_t *cm_points;
    size_t lut_size;
};
typedef struct lut_cfg *lut_cfg_t;

class DistansceSensor
{
private:
    gp2y0a21_handle_t handle;
    uint32_t distance;

    // Filtering buffer
    static constexpr int FILTER_SIZE = 5;
    uint32_t buffer[FILTER_SIZE];
    int index = 0;
    bool filled = false;

public:
    DistansceSensor(adc_unit_t unit,
                    adc_channel_t channel,
                    adc_atten_t atten = ADC_ATTEN_DB_12,
                    uint8_t samples = 20,
                    lut_cfg_t lut = nullptr);

    ~DistansceSensor();

    // Single read (already includes median+IIR inside driver)
    uint32_t readDistance();

    // Raw voltage (mV)
    int readVoltage();

    // Rolling average filter on top of readDistance()
    uint32_t readDistanceFiltered();
};
