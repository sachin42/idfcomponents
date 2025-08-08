#pragma once

#include "WString.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include <functional>

class OTAUpdater
{
public:
    using ProgressCallback = std::function<void(int progress, int total, int percent)>;
    using CompletionCallback = std::function<void()>;
    using BeforeStartCallback = std::function<void()>;

    static esp_err_t init(const String &url);
    static esp_err_t start();
    static void setProgressCallback(ProgressCallback cb);
    static void setCompletionCallback(CompletionCallback cb);
    static void setBeforeStartCallback(BeforeStartCallback cb);
    static int getProgress();

private:
    OTAUpdater() = default;

    static void run(void *arg);
    static bool headerChecked;
    static bool isRunning;
    static int binaryFileLength;
    static String otaUrl;
    static TaskHandle_t otaTaskHandle;

    static ProgressCallback progressCb;
    static CompletionCallback completeCb;
    static BeforeStartCallback beforeStartCb;
};
