/*
 * BTAddress.h
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 *  Ported  on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#ifndef COMPONENTS_CPP_UTILS_BTADDRESS_H_
#define COMPONENTS_CPP_UTILS_BTADDRESS_H_
#include "sdkconfig.h"
#include "WString.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_bt_api.h>  // ESP32 BT
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_timer.h>

#define millis() (uint32_t)esp_timer_get_time()/1000
#define delay(x) vTaskDelay(pdMS_TO_TICKS(x))

#define LOG_TAG "BTSERIAL"

#define log_e(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
#define log_v(...) ESP_LOGV(LOG_TAG, __VA_ARGS__)
#define log_i(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
#define log_d(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
#define log_w(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)


/**
 * @brief A %BT device address.
 *
 * Every %BT device has a unique address which can be used to identify it and form connections.
 */
class BTAddress {
public:
  BTAddress();
  BTAddress(esp_bd_addr_t address);
  BTAddress(String stringAddress);
  bool equals(BTAddress otherAddress);
  operator bool() const;

  esp_bd_addr_t *getNative() const;
  String toString(bool capital = false) const;

private:
  esp_bd_addr_t m_address;
};

#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BTADDRESS_H_ */
