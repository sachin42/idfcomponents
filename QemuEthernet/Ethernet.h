#pragma once

#include "WString.h" // Arduino-style String
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_eth_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class EthernetManager
{
public:
    EthernetManager();
    ~EthernetManager();

    bool begin();                            // start Ethernet and wait for IP
    void end();                              // stop Ethernet and release
    bool isConnected() { return connected; } // true if got IP
    String LocalIP();
    String Netmask();
    String Gateway();

private:
    static void onGotIP(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    esp_netif_t *netif;
    esp_eth_handle_t ethHandle;
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
    esp_eth_netif_glue_handle_t glue;

    SemaphoreHandle_t gotIpSem;
    bool connected;
};

extern EthernetManager Eth;