# Blynk Library for ESP-IDF

A port of the Arduino Blynk legacy library to ESP-IDF (FreeRTOS) with full thread-safe API, non-blocking TCP transport, and built-in task management.

## Features

- **Thread-Safe API**: All `Blynk.*` calls are internally protected by recursive mutex — no user-side locking required
- **Dedicated Task Management**: Single `BlynkTaskStart()` call spawns a managed FreeRTOS task (core 0, priority 5)
- **Non-Blocking Transport**: Asynchronous TCP connect, read, and write using `select()` and non-blocking sockets
- **BlynkTimer Integration**: 16-slot timer driven inside the Blynk task (avoids mutex contention)
- **FreeRTOS Native**: Pure lwip/POSIX sockets, no Arduino layer — works on bare ESP32/IDF
- **Event Handlers**: `BLYNK_WRITE`, `BLYNK_CONNECTED`, `BLYNK_DISCONNECTED` macros for responsive UI control
- **Virtual Pins**: Send/receive data to/from Blynk app via `Blynk.virtualWrite()` and `BLYNK_WRITE(Vx)` handlers

## Installation

### Add to idf_component.yml

```yaml
dependencies:
  sachin42/blynk: "1.2.0"
```

### Update CMakeLists.txt

```cmake
idf_component_register(
    REQUIRES esp_http_client sachin42__blynk sachin42__print
    SRCS main.cpp
    INCLUDE_DIRS .
)
```

### Verify Component Installation

```bash
idf.py recycle
```

## Quick Start

```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Ethernet.h"  // or WiFi.h if using WiFi
#include "BlynkEspIDF.h"

BlynkTimer timer;

// Handler for button presses on V1
BLYNK_WRITE(V1) {
    int value = param.asInt();
    ESP_LOGI("APP", "Button V1: %d", value);
}

// Timer callback — runs inside Blynk task
void send_heartbeat() {
    Blynk.virtualWrite(V2, "online");  // Thread-safe, no lock needed
}

extern "C" void app_main(void) {
    // Initialize Ethernet (or WiFi)
    Eth.begin();
    
    // Register timer callback (before BlynkTaskStart to avoid race)
    timer.setInterval(5000, send_heartbeat);
    
    // Start Blynk task: auth token, server, port, timer, stack, priority, core
    BlynkTaskStart("YourAuthToken", "blynk.cloud.blynk.io", 8080, &timer, 8192, 5, 0);
    
    // app_main completes; Blynk task runs independently
    vTaskDelete(NULL);
}
```

## API Reference

### BlynkTaskStart()

Spawns a dedicated FreeRTOS task that runs `Blynk.run()` and optionally `timer.run()`.

```cpp
TaskHandle_t BlynkTaskStart(
    const char *auth,           // Authentication token from Blynk app
    const char *domain,         // Server hostname (e.g., "blynk.cloud.blynk.io")
    uint16_t port,              // Server port (typically 8080 or 443)
    BlynkTimer *timer = nullptr,  // Optional BlynkTimer instance for callbacks
    uint32_t stack = 8192,      // FreeRTOS task stack size in bytes (default 8 KB)
    UBaseType_t prio = 5,       // FreeRTOS task priority (default 5)
    BaseType_t core = 0         // CPU core to pin task (0=PRO_CPU, 1=APP_CPU)
);

// Returns: TaskHandle to the created Blynk task, or nullptr on failure
```

**Defaults**: Stack 8 KB, priority 5, core 0. Recommended for typical IoT applications.

**Important**: Do NOT call `Blynk.run()` in your `app_main()` loop — the task handles it.

### Virtual Pin Handlers

#### BLYNK_WRITE(Vx)

Triggered when the Blynk app sends a value to virtual pin `x`. Handler runs inside the Blynk task.

```cpp
BLYNK_WRITE(V1) {
    int value = param.asInt();
    // Process value
    Blynk.virtualWrite(V2, value * 2);  // Safe to call Blynk API
}

BLYNK_WRITE(V3) {
    const char *msg = param.asString();
    // Handle string data
}
```

**Available param methods**:
- `param.asInt()` — Integer value
- `param.asDouble()` — Floating-point value
- `param.asString()` — String data (buffer)
- `param.getBuffer()` — Raw byte buffer

#### BLYNK_CONNECTED()

Triggered every time the device connects or reconnects to the Blynk server.

```cpp
static bool timers_initialized = false;

BLYNK_CONNECTED() {
    // Guard against registering duplicate timers on each reconnect
    if (!timers_initialized) {
        timer.setInterval(10000, update_sensor_data);
        timers_initialized = true;
    }
    
    // Refresh UI state
    Blynk.syncVirtual(V1, V2, V3);
    Blynk.setProperty(V1, "label", "Status: Online");
}
```

**Important**: This handler fires on every reconnect. Use a static guard variable to prevent timer slot leaks.

#### BLYNK_DISCONNECTED()

Triggered when the connection to Blynk server is lost.

```cpp
BLYNK_DISCONNECTED() {
    ESP_LOGW("BLYNK", "Disconnected from server");
    // Optional: perform cleanup or notify other tasks
}
```

### Core Blynk API Methods

All methods are thread-safe and can be called from any FreeRTOS task.

```cpp
// Virtual pin communication
Blynk.virtualWrite(int pin, const char *fmt, ...);
Blynk.virtualWrite(int pin, double value);
Blynk.virtualWrite(int pin, String str);

// Widget control (property updates)
Blynk.setProperty(int pin, const char *property, const char *value);
Blynk.setProperty(int pin, const char *property, double value);

// Sync virtual pins from server
Blynk.syncVirtual(int pin1, int pin2, ...);

// Connection status
bool Blynk.connected();

// Low-level message sending
Blynk.sendInternal(const char *cmd, ...);
```

## BlynkTimer Usage

The `BlynkTimer` class provides 16 timer slots for non-blocking interval/one-shot callbacks.

### setInterval()

Register a callback to run every `interval` milliseconds.

```cpp
BlynkTimer timer;

void my_callback() {
    Blynk.virtualWrite(V1, "tick");
}

void setup() {
    // Register BEFORE BlynkTaskStart to avoid race condition
    int timerID = timer.setInterval(1000, my_callback);
}
```

**Returns**: Timer ID (0–15) on success, `-1` if no free slots (max 16 timers).

**Important**: Register all timers before calling `BlynkTaskStart()` or use a static guard in `BLYNK_CONNECTED()`.

### setTimeout()

Register a callback to run once after `delay` milliseconds.

```cpp
timer.setTimeout(2000, []() {
    Blynk.virtualWrite(V2, "delayed");
});
```

### deleteTimer() / disable() / enable()

```cpp
int id = timer.setInterval(1000, callback);
timer.disable(id);  // Pause callback (still holds a slot)
timer.enable(id);   // Resume callback
timer.deleteTimer(id);  // Release slot
```

### Timer Slot Management

Max 16 slots. Avoid leaks by:

1. **Static guard in BLYNK_CONNECTED()**:
   ```cpp
   static bool initialized = false;
   BLYNK_CONNECTED() {
       if (!initialized) {
           timer.setInterval(1000, cb);
           initialized = true;
       }
   }
   ```

2. **Register all timers before BlynkTaskStart()**:
   ```cpp
   timer.setInterval(1000, cb1);
   timer.setInterval(2000, cb2);
   BlynkTaskStart(..., &timer, ...);
   ```

3. **Explicitly deleteTimer() on reconnect** (if needed):
   ```cpp
   // Store IDs in static variables and delete them before re-registering
   ```

## Thread Safety

### Thread-Safe Operations

All `Blynk.*` API calls are internally protected by a recursive mutex (`BlynkApiLockGuard`). You can call them from any FreeRTOS task without additional locking:

```cpp
// Any task can safely call these:
Blynk.virtualWrite(V1, 42);
Blynk.setProperty(V2, "label", "Value");
Blynk.connected();
Blynk.syncVirtual(V1, V2);
```

### NOT Safe from ISR

**Do NOT call Blynk API from interrupt handlers (ISR)**. Use FreeRTOS queues to defer work:

```cpp
// ISR context: send to queue
void gpio_isr_handler(void *arg) {
    xQueueSendFromISR(queue, &event, NULL);
}

// Task context: consume from queue and call Blynk API
static void event_handler_task(void *arg) {
    for (;;) {
        if (xQueueReceive(queue, &event, portMAX_DELAY)) {
            Blynk.virtualWrite(V1, event.value);  // Safe in task
        }
    }
}
```

### Handler Execution Context

`BLYNK_WRITE()`, `BLYNK_CONNECTED()`, and `BLYNK_DISCONNECTED()` handlers run **inside the Blynk task** (core 0, priority 5). You can safely call any Blynk API from these handlers.

```cpp
BLYNK_WRITE(V1) {
    int val = param.asInt();
    // This callback runs in Blynk task — all Blynk API calls are safe
    Blynk.virtualWrite(V2, val);
    Blynk.setProperty(V3, "label", "Updated");
}
```

### BlynkLockGuard & BLYNK_LOCK/UNLOCK

These are **no-ops** for backward compatibility:

```cpp
// These do nothing (not needed anymore)
BlynkLockGuard guard;
BLYNK_LOCK();
Blynk.virtualWrite(V1, 42);
BLYNK_UNLOCK();

// Just call the API directly:
Blynk.virtualWrite(V1, 42);
```

## Parallel Tasks

The Blynk task runs on core 0 (PRO_CPU) with priority 5. You can spawn additional tasks on core 1 (APP_CPU) for CPU-intensive work without blocking Blynk network operations.

### Example: HTTP Task on Core 1

```cpp
#include "esp_http_client.h"

static void http_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(3000));  // Wait for Blynk to connect
    
    esp_http_client_config_t config = {};
    config.url = "http://api.example.com/data";
    config.timeout_ms = 8000;
    
    for (;;) {
        esp_http_client_handle_t client = esp_http_client_init(&config);
        
        if (esp_http_client_perform(client) == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            // Call Blynk API from this task — thread-safe!
            Blynk.virtualWrite(V7, status);
        }
        
        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

extern "C" void app_main(void) {
    Eth.begin();
    timer.setInterval(1000, cb);
    BlynkTaskStart("YourAuthToken", "blynk.server.com", 8080, &timer);
    
    // Spawn HTTP task on core 1 (non-network work)
    xTaskCreatePinnedToCore(http_task, "http", 8192, NULL, 4, NULL, 1);
    
    vTaskDelete(NULL);
}
```

### Core Allocation Recommendation

| Task | Core | Priority | Notes |
|------|------|----------|-------|
| Blynk (network) | 0 | 5 | Managed by `BlynkTaskStart()` |
| HTTP / external API | 1 | 4 | CPU-intensive, parallel to Blynk |
| Sensor reading | 1 | 4 | I/O-bound work on APP_CPU |
| Real-time ISR | 0 or 1 | 15+ | Keep ISR duration < 50 µs |

## Configuration

### Compile-Time Defines

Define these in your `CMakeLists.txt` or `sdkconfig` before including BlynkEspIDF headers:

```cpp
// Connection timeout for socket connect (default: 8000 ms)
#define BLYNK_CONNECT_TIMEOUT_MS 10000

// Poll interval for incoming data (default: 50 ms)
#define BLYNK_READ_POLL_MS 50

// Poll interval for sending data (default: 2000 ms)
#define BLYNK_SEND_POLL_MS 2000

// Heartbeat interval (default: 50 s)
#define BLYNK_HEARTBEAT 50

// Max message limit per period (default: 20)
#define BLYNK_MSG_LIMIT 20

// Max incoming payload size (default: 2048 bytes)
#define BLYNK_MAX_READBYTES 2048

// Enable debug logging
#define BLYNK_DEBUG
#define BLYNK_PRINT stdout  // stdout or your_log_function
```

### Set in CMakeLists.txt

```cmake
idf_component_register(
    REQUIRES sachin42__blynk
    SRCS main.cpp
)

# Add compile flags
target_compile_options(${COMPONENT_NAME} PRIVATE -DBLYNK_DEBUG -DBLYNK_CONNECT_TIMEOUT_MS=10000)
```

## Troubleshooting

### Connection Hangs or Timeout

**Symptom**: `BlynkTaskStart()` never completes or times out after 8 seconds.

**Causes**:
- DNS resolution failure (server unreachable)
- Network not initialized (call `Eth.begin()` before `BlynkTaskStart()`)
- Firewall blocking port 8080 or 443

**Solution**:
- Enable `BLYNK_DEBUG` and check log output
- Increase `BLYNK_CONNECT_TIMEOUT_MS` if network is slow
- Verify server hostname and port are correct
- Test DNS with `getaddrinfo()` in a separate task

### Timer Slot Leaks

**Symptom**: Timers stop firing after several reconnects.

**Cause**: `BLYNK_CONNECTED()` registers duplicate timers each reconnect (max 16 slots).

**Solution**: Use static guard:
```cpp
BLYNK_CONNECTED() {
    static bool initialized = false;
    if (!initialized) {
        timer.setInterval(1000, callback);
        initialized = true;
    }
}
```

### "Invalid pin" or Handler Not Firing

**Symptom**: `BLYNK_WRITE(Vx)` handler never called when app sends value.

**Causes**:
- Macro not defined (check header includes)
- Virtual pin number doesn't match (e.g., `BLYNK_WRITE(V1)` but app uses V2)
- Connection not established

**Solution**:
```cpp
#include "BlynkEspIDF.h"  // Required!

BLYNK_WRITE(V1) { ... }  // Must match app widget pin
```

### Mutex Deadlock from ISR

**Symptom**: Task hangs or watchdog reset after ISR calls Blynk API.

**Cause**: ISR called `Blynk.virtualWrite()` while Blynk task holds recursive mutex.

**Solution**: Use FreeRTOS queue to defer work to a regular task:
```cpp
void gpio_isr_handler(void *arg) {
    xQueueSendFromISR(event_queue, &event, NULL);
}

static void event_task(void *arg) {
    for (;;) {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {
            Blynk.virtualWrite(V1, event.value);  // Safe
        }
    }
}
```

### Memory Issues / Stack Overflow

**Symptom**: Blynk task crashes or restarts on first message.

**Cause**: Stack too small (8 KB may not be enough if using large buffers or deep call chains).

**Solution**: Increase stack size:
```cpp
BlynkTaskStart(token, server, port, &timer, 12288, 5, 0);  // 12 KB instead of 8 KB
```

Typical usage: 8–12 KB. Monitor free stack with `uxTaskGetStackHighWaterMark(task_handle)`.

## Examples

### Send Sensor Data Every 10 Seconds

```cpp
float read_temperature() {
    // Read from sensor...
    return 25.5;
}

void send_temp() {
    Blynk.virtualWrite(V1, read_temperature());
}

extern "C" void app_main(void) {
    Eth.begin();
    timer.setInterval(10000, send_temp);
    BlynkTaskStart("YourToken", "blynk.cloud.blynk.io", 8080, &timer);
    vTaskDelete(NULL);
}
```

### Handle Button with Feedback

```cpp
BLYNK_WRITE(V2) {
    if (param.asInt() == 1) {
        // Button pressed
        Blynk.virtualWrite(V3, "Button pressed!");
    }
}
```

### Sync Device State on Reconnect

```cpp
BLYNK_CONNECTED() {
    Blynk.syncVirtual(V1, V2, V3);  // Fetch latest values from app
}

BLYNK_READ(V4) {
    Blynk.virtualWrite(V4, get_current_temperature());
}
```

## License

MIT — See LICENSE file in this repository.

## Support

- Report issues on [GitHub](https://github.com/sachin42/idfcomponents)
- Blynk legacy server documentation: [Blynk Protocol](https://docs.blynk.io/)
