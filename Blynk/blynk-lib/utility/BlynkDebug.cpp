/**
 * @file       BlynkDebug.cpp
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Jan 2015
 * @brief      Debug utilities for Arduino
 */
#include <Blynk/BlynkDebug.h>
#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include "esp_timer.h"


void BlynkDelay(millis_time_t ms)
{
    return vTaskDelay(pdMS_TO_TICKS(ms));
}



millis_time_t BlynkMillis()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
    ;
}



size_t BlynkFreeRam()
{
    return esp_get_free_heap_size();
}



void BlynkReset()
{
    esp_restart();
}


void BlynkFatal()
{
    BlynkDelay(10000L);
    BlynkReset();
}
