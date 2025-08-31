#include "Ethernet.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ETH";
EthernetManager Eth;

EthernetManager::EthernetManager()
    : netif(nullptr), ethHandle(nullptr), mac(nullptr), phy(nullptr), glue(nullptr),
      gotIpSem(nullptr), connected(false)
{
}

EthernetManager::~EthernetManager()
{
    end();
}

bool EthernetManager::begin()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (gotIpSem == nullptr)
        gotIpSem = xSemaphoreCreateBinary();

    // Init netif
    esp_netif_inherent_config_t netif_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    netif_cfg.if_desc = "eth0";
    netif_cfg.route_prio = 64;

    esp_netif_config_t cfg = {
        .base = &netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    netif = esp_netif_new(&cfg);

    // MAC + PHY
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 2048;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    // phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;

    mac = esp_eth_mac_new_openeth(&mac_config);
    phy = esp_eth_phy_new_dp83848(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &ethHandle));

    uint8_t eth_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(eth_mac, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(ethHandle, ETH_CMD_S_MAC_ADDR, eth_mac));

    glue = esp_eth_new_netif_glue(ethHandle);
    esp_netif_attach(netif, glue);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetManager::onGotIP, this));
    ESP_ERROR_CHECK(esp_eth_start(ethHandle));

    // Wait for IP
    if (xSemaphoreTake(gotIpSem, pdMS_TO_TICKS(10000)) == pdTRUE)
    {
        connected = true;
        return true;
    }

    ESP_LOGE(TAG, "Ethernet connection timeout");
    return false;
}

void EthernetManager::end()
{
    if (ethHandle)
    {
        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetManager::onGotIP));
        ESP_ERROR_CHECK(esp_eth_stop(ethHandle));
        ESP_ERROR_CHECK(esp_eth_del_netif_glue(glue));
        ESP_ERROR_CHECK(esp_eth_driver_uninstall(ethHandle));
        ethHandle = nullptr;
    }

    if (phy)
    {
        ESP_ERROR_CHECK(phy->del(phy));
        phy = nullptr;
    }
    if (mac)
    {
        ESP_ERROR_CHECK(mac->del(mac));
        mac = nullptr;
    }
    if (netif)
    {
        esp_netif_destroy(netif);
        netif = nullptr;
    }

    if (gotIpSem)
    {
        vSemaphoreDelete(gotIpSem);
        gotIpSem = nullptr;
    }

    connected = false;
}

String EthernetManager::LocalIP()
{
    if (!netif || !connected)
        return "";

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        char buf[16];
        esp_ip4addr_ntoa(&ip_info.ip, buf, sizeof(buf));
        return String(buf);
    }
    return "";
}

String EthernetManager::Netmask()
{
    if (!netif || !connected)
        return "";

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        char buf[16];
        esp_ip4addr_ntoa(&ip_info.netmask, buf, sizeof(buf));
        return String(buf);
    }
    return "";
}

String EthernetManager::Gateway()
{
    if (!netif || !connected)
        return "";

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        char buf[16];
        esp_ip4addr_ntoa(&ip_info.gw, buf, sizeof(buf));
        return String(buf);
    }
    return "";
}

void EthernetManager::onGotIP(void *arg, esp_event_base_t, int32_t, void *event_data)
{
    auto *self = static_cast<EthernetManager *>(arg);
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IPv4: " IPSTR, IP2STR(&event->ip_info.ip));
    if (self->gotIpSem)
        xSemaphoreGive(self->gotIpSem);
}
