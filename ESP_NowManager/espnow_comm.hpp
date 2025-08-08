// espnow_manager.hpp
#pragma once

#include <esp_now.h>
#include <esp_wifi.h>
#include <functional>
#include <vector>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

enum class MessageType : uint8_t {
    DATA = 0x01,
    ACK = 0x02
};

struct EspNowPacket {
    MessageType type;
    uint16_t packetId;
    uint16_t checksum;
    uint8_t payload[200];
    size_t payloadLen;
};

struct PendingPacket {
    uint8_t targetMac[ESP_NOW_ETH_ALEN];
    EspNowPacket packet;
    int retriesLeft;
    TickType_t timeoutTick;
    bool ackReceived;
};

class EspNowManager {
public:
    EspNowManager();
    bool begin(const uint8_t *mac);

    esp_err_t sendWithAck(const uint8_t* data, size_t len);
    void registerCommandHandler(std::function<void(const uint8_t* mac, const uint8_t* data, size_t len)> handler);

private:
    static void onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);
    static void onSend(const uint8_t* mac_addr, esp_now_send_status_t status);

    void processReceivedPacket(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void pendingTask(void* pvParameter);

    static uint16_t computeChecksum(const EspNowPacket& packet);

    static EspNowManager* instance;
    std::vector<PendingPacket> pendingPackets;
    std::function<void(const uint8_t* mac, const uint8_t* data, size_t len)> commandHandler;
    SemaphoreHandle_t pendingMutex;
    TaskHandle_t pendingTaskHandle;

    static constexpr int MAX_RETRIES = 3;
    static constexpr int ACK_TIMEOUT_MS = 300;

    uint8_t targetMac[ESP_NOW_ETH_ALEN];
};
