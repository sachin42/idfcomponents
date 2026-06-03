#ifndef BLYNK_ESP32_IDF_H
#define BLYNK_ESP32_IDF_H
#define BLYNK_NO_BUILTIN

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "BlynkApiEsp32.h"
#include "BlynkSocketEsp32.h"

extern BlynkSocket Blynk;

// All Blynk.* methods are internally thread-safe (recursive mutex inside
// BlynkProtocol). You do NOT need to lock when calling Blynk APIs from
// any FreeRTOS task. Just call Blynk.virtualWrite(...) directly.
//
// The macros / guard below are kept as no-ops for backward compatibility
// with code written against the older external-locking API.
#define BLYNK_LOCK()   ((void)0)
#define BLYNK_UNLOCK() ((void)0)
class BlynkLockGuard { public: BlynkLockGuard() {} ~BlynkLockGuard() {} };

class BlynkTimer;

// Spawn dedicated FreeRTOS task running Blynk.run() (+ optional timer.run()).
// Recommended: pin to PRO_CPU (core 0), priority 5, stack ≥ 8 KB.
TaskHandle_t BlynkTaskStart(const char *auth,
                            const char *domain,
                            uint16_t port,
                            BlynkTimer *timer = nullptr,
                            uint32_t stack = 8192,
                            UBaseType_t prio = 5,
                            BaseType_t core = 0);

#endif
