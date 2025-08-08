// espnow_manager.cpp
#include "espnow_comm.hpp"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>

#define TAG "ESPNOW"

EspNowManager *EspNowManager::instance = nullptr;

EspNowManager::EspNowManager()
{
    pendingMutex = xSemaphoreCreateMutex();
    instance = this;
}

bool EspNowManager::begin(const uint8_t *mac)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));

    if (esp_now_init() != ESP_OK)
        return false;
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSend);

    // Store target MAC
    memcpy(this->targetMac, mac, ESP_NOW_ETH_ALEN);

    // Add peer
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return ESP_FAIL;
    }

    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = 1;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;

    memcpy(peer->peer_addr, mac, ESP_NOW_ETH_ALEN);

    esp_err_t ret = esp_now_add_peer(peer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Add peer failed: %s", esp_err_to_name(ret));
        free(peer);
        return false;
    }

    free(peer);
    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    xTaskCreatePinnedToCore(pendingTask, "PendingACKTask", 4096, this, 1, &pendingTaskHandle, 1);
    return true;
}

esp_err_t EspNowManager::sendWithAck(const uint8_t *data, size_t len)
{
    EspNowPacket pkt = {};
    pkt.type = MessageType::DATA;
    pkt.packetId = esp_random();
    memcpy(pkt.payload, data, len);
    pkt.payloadLen = len;
    pkt.checksum = computeChecksum(pkt);

    PendingPacket pending;
    memcpy(pending.targetMac, targetMac, ESP_NOW_ETH_ALEN);
    pending.packet = pkt;
    pending.retriesLeft = MAX_RETRIES;
    pending.timeoutTick = xTaskGetTickCount() + pdMS_TO_TICKS(ACK_TIMEOUT_MS);
    pending.ackReceived = false;

    xSemaphoreTake(pendingMutex, portMAX_DELAY);
    pendingPackets.push_back(pending);
    xSemaphoreGive(pendingMutex);

    return esp_now_send(targetMac, (uint8_t *)&pkt, sizeof(pkt));
}

void EspNowManager::registerCommandHandler(std::function<void(const uint8_t *mac, const uint8_t *data, size_t len)> handler)
{
    commandHandler = handler;
}

void EspNowManager::onReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len)
{
    if (instance)
        instance->processReceivedPacket(esp_now_info, data, len);
}

void EspNowManager::onSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // Optional: log success/failure
}

void EspNowManager::processReceivedPacket(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (memcmp(info->src_addr, targetMac, ESP_NOW_ETH_ALEN) != 0) {
        ESP_LOGW(TAG, "Received packet from unknown MAC — ignored");
        return;
    }

    if (len < sizeof(EspNowPacket) - 200)
        return;
    EspNowPacket pkt;
    memcpy(&pkt, data, len);

    if (pkt.type == MessageType::ACK)
    {
        xSemaphoreTake(pendingMutex, portMAX_DELAY);
        for (auto &pending : pendingPackets)
        {
            if (pending.packet.packetId == pkt.packetId)
            {
                pending.ackReceived = true;
                break;
            }
        }
        xSemaphoreGive(pendingMutex);
    }
    else if (pkt.type == MessageType::DATA)
    {
        // Validate checksum
        if (pkt.checksum != computeChecksum(pkt))
            return;

        if (commandHandler)
            commandHandler(info->src_addr, pkt.payload, pkt.payloadLen);

        // Send ACK
        EspNowPacket ackPkt;
        ackPkt.type = MessageType::ACK;
        ackPkt.packetId = pkt.packetId;
        ackPkt.payloadLen = 0;
        ackPkt.checksum = computeChecksum(ackPkt);
        esp_now_send(info->src_addr, (uint8_t *)&ackPkt, sizeof(ackPkt));
    }
}

void EspNowManager::pendingTask(void *pvParameter)
{
    EspNowManager *self = static_cast<EspNowManager *>(pvParameter);
    while (true)
    {
        xSemaphoreTake(self->pendingMutex, portMAX_DELAY);
        for (auto it = self->pendingPackets.begin(); it != self->pendingPackets.end();)
        {
            if (it->ackReceived)
            {
                it = self->pendingPackets.erase(it);
            }
            else if (xTaskGetTickCount() >= it->timeoutTick)
            {
                if (it->retriesLeft > 0)
                {
                    esp_now_send(it->targetMac, (uint8_t *)&it->packet, sizeof(it->packet));
                    it->retriesLeft--;
                    it->timeoutTick = xTaskGetTickCount() + pdMS_TO_TICKS(ACK_TIMEOUT_MS);
                    ++it;
                }
                else
                {
                    // Max retries reached, drop packet
                    it = self->pendingPackets.erase(it);
                }
            }
            else
            {
                ++it;
            }
        }
        xSemaphoreGive(self->pendingMutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

uint16_t EspNowManager::computeChecksum(const EspNowPacket &packet)
{
    uint16_t sum = static_cast<uint8_t>(packet.type) + packet.packetId;
    for (size_t i = 0; i < packet.payloadLen; ++i)
        sum += packet.payload[i];
    return sum;
}
