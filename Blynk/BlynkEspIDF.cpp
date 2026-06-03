#include "BlynkEspIDF.h"
#include "Blynk/BlynkTimer.h"
#include "freertos/task.h"

static BlynkTransportEsp32 _blynkTransport;
BlynkSocket Blynk(_blynkTransport);

struct BlynkTaskArgs {
    const char *auth;
    const char *domain;
    uint16_t    port;
    BlynkTimer *timer;
};

static void blynk_task_entry(void *pv)
{
    BlynkTaskArgs *args = static_cast<BlynkTaskArgs *>(pv);

    Blynk.begin(args->auth, args->domain, args->port);

    BlynkTimer *t = args->timer;
    vPortFree(args);

    for (;;)
    {
        Blynk.run();
        if (t) t->run();
        vTaskDelay(1);
    }
}

TaskHandle_t BlynkTaskStart(const char *auth,
                            const char *domain,
                            uint16_t port,
                            BlynkTimer *timer,
                            uint32_t stack,
                            UBaseType_t prio,
                            BaseType_t core)
{
    BlynkTaskArgs *args = static_cast<BlynkTaskArgs *>(pvPortMalloc(sizeof(BlynkTaskArgs)));
    if (!args) return nullptr;
    args->auth   = auth;
    args->domain = domain;
    args->port   = port;
    args->timer  = timer;

    TaskHandle_t h = nullptr;
    BaseType_t ok = xTaskCreatePinnedToCore(blynk_task_entry, "blynk",
                                            stack, args, prio, &h, core);
    if (ok != pdPASS) {
        vPortFree(args);
        return nullptr;
    }
    return h;
}
