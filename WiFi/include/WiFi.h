#pragma once

#include "WString.h"
#include "sdkconfig.h"
#include <esp_err.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

class WiFiManager
{
public:
    WiFiManager() = default;
    ~WiFiManager() = default;

    void begin(const char * ssid = nullptr, const char * pass = nullptr);
    void reset();
    bool isConnected() const;
    String getServiceName(size_t max, const String &prefix);
    // New getters for network info
    String LocalIP() const { return ipAddress; }
    String Netmask() const { return netmask; }
    String Gateway() const { return gateway; }

private:
    static void eventHandler(void *arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void *event_data);

    void initNetif();
    void initEventLoop();

    static EventGroupHandle_t wifi_event_group;
    static constexpr EventBits_t WIFI_CONNECTED_EVENT = BIT0;
    // Store IP information
    static String ipAddress;
    static String netmask;
    static String gateway;
};

// Global instance like Arduino Serial
extern WiFiManager WiFi;
