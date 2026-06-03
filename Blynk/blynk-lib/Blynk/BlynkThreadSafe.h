/**
 * @file       BlynkThreadSafe.h
 * @brief      Internal recursive lock used by BlynkProtocol so every public
 *             Blynk API call is safe to invoke from any FreeRTOS task.
 *
 *             The lock is a recursive mutex, so:
 *               - run() takes it, processes input, dispatches BLYNK_WRITE
 *                 handlers which may call virtualWrite()/sendCmd() — those
 *                 re-enter the lock without deadlocking.
 *               - External tasks calling Blynk.virtualWrite() etc. block
 *                 only while another caller (or the Blynk task) is mid-send.
 */
#ifndef BlynkThreadSafe_h
#define BlynkThreadSafe_h

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// C++11 guarantees thread-safe initialization of function-local statics.
// First caller creates the mutex; subsequent callers reuse it.
inline SemaphoreHandle_t _blynkGetMutex()
{
    static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
    return m;
}

class BlynkApiLockGuard
{
public:
    BlynkApiLockGuard()
    {
        xSemaphoreTakeRecursive(_blynkGetMutex(), portMAX_DELAY);
    }
    ~BlynkApiLockGuard()
    {
        xSemaphoreGiveRecursive(_blynkGetMutex());
    }
private:
    BlynkApiLockGuard(const BlynkApiLockGuard&);
    BlynkApiLockGuard& operator=(const BlynkApiLockGuard&);
};

#define BLYNK_API_GUARD() BlynkApiLockGuard _blynk_api_guard_

#endif
