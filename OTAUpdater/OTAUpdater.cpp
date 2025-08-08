#include "OTAUpdater.hpp"

#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_flash_partitions.h"
#include "esp_system.h"
#include "esp_app_format.h"
#include <cstdio>
#include "esp_ota_ops.h"
#include <cstring>
#include <cassert>

#define BUFFSIZE 1024

static const char *TAG = "OTAUpdater";
static char otaWriteBuffer[BUFFSIZE + 1] = {0};

// Static members
bool OTAUpdater::headerChecked = false;
bool OTAUpdater::isRunning = false;
int OTAUpdater::binaryFileLength = 0;
String OTAUpdater::otaUrl;
TaskHandle_t OTAUpdater::otaTaskHandle = nullptr;
OTAUpdater::ProgressCallback OTAUpdater::progressCb = nullptr;
OTAUpdater::CompletionCallback OTAUpdater::completeCb = nullptr;
OTAUpdater::BeforeStartCallback OTAUpdater::beforeStartCb = nullptr;

esp_err_t OTAUpdater::init(const String &url) {
    if (isRunning) {
        ESP_LOGW(TAG, "OTA already running. Ignoring init.");
        return ESP_ERR_INVALID_STATE;
    }

    otaUrl = url;
    return ESP_OK;
}

void OTAUpdater::setProgressCallback(ProgressCallback cb) {
    progressCb = cb;
}

void OTAUpdater::setCompletionCallback(CompletionCallback cb) {
    completeCb = cb;
}

void OTAUpdater::setBeforeStartCallback(BeforeStartCallback cb) {
    beforeStartCb = cb;
}

int OTAUpdater::getProgress() {
    return binaryFileLength;
}

esp_err_t OTAUpdater::start() {
    if (isRunning) {
        ESP_LOGW(TAG, "OTA already running");
        return ESP_ERR_INVALID_STATE;
    }

    if (beforeStartCb) {
        beforeStartCb();
    }

    isRunning = true;
    headerChecked = false;
    binaryFileLength = 0;

    BaseType_t created = xTaskCreate(OTAUpdater::run, "ota_task", 8192, nullptr, 5, &otaTaskHandle);
    return (created == pdPASS) ? ESP_OK : ESP_FAIL;
}

void OTAUpdater::run(void *arg) {
    esp_http_client_config_t config = {
        .url = otaUrl.c_str(),
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client || esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        isRunning = false;
        vTaskDelete(NULL);
        return;
    }

    esp_http_client_fetch_headers(client);

    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(NULL);
    assert(updatePartition);

    esp_ota_handle_t updateHandle;
    esp_err_t err;

    while (1) {
        int data_read = esp_http_client_read(client, otaWriteBuffer, BUFFSIZE);
        if (data_read < 0) {
            ESP_LOGE(TAG, "HTTP read error");
            esp_http_client_cleanup(client);
            isRunning = false;
            vTaskDelete(NULL);
        } else if (data_read > 0) {
            if (!headerChecked) {
                if (data_read < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    ESP_LOGE(TAG, "Header too short");
                    esp_http_client_cleanup(client);
                    isRunning = false;
                    vTaskDelete(NULL);
                }

                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, &otaWriteBuffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));

                const esp_partition_t *running = esp_ota_get_running_partition();
                esp_app_desc_t running_app_info;
                if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK &&
                    memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
                    ESP_LOGW(TAG, "Same firmware version. Aborting.");
                    esp_http_client_cleanup(client);
                    isRunning = false;
                    vTaskDelete(NULL);
                }

                err = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &updateHandle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin failed");
                    esp_http_client_cleanup(client);
                    isRunning = false;
                    vTaskDelete(NULL);
                }

                headerChecked = true;
            }

            if (esp_ota_write(updateHandle, (const void *)otaWriteBuffer, data_read) != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed");
                esp_ota_abort(updateHandle);
                esp_http_client_cleanup(client);
                isRunning = false;
                vTaskDelete(NULL);
            }

            binaryFileLength += data_read;

            if (progressCb) {
                int total = esp_http_client_get_content_length(client);
                static int lastPercent = -1;
                if (total > 0) {
                    int percent = (binaryFileLength * 100) / total;
                    if (percent != lastPercent) {
                        lastPercent = percent;
                        progressCb(binaryFileLength, total, percent);
                    }
                } else {
                    progressCb(binaryFileLength, 0, 0);
                }
            }
        } else {
            if (esp_http_client_is_complete_data_received(client))
                break;
        }
    }

    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "Incomplete OTA image");
        esp_ota_abort(updateHandle);
        esp_http_client_cleanup(client);
        isRunning = false;
        vTaskDelete(NULL);
    }

    if (esp_ota_end(updateHandle) != ESP_OK || esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
        ESP_LOGE(TAG, "OTA commit failed");
        esp_http_client_cleanup(client);
        isRunning = false;
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "OTA completed. Ready for restart.");

    esp_http_client_cleanup(client);
    if (completeCb) completeCb();

    isRunning = false;
    vTaskDelete(NULL);
}

