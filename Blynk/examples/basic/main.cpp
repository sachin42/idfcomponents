/**
 * @file main.cpp
 * @brief Basic Blynk ESP-IDF example with Ethernet, timer callbacks, and parallel tasks.
 *
 * This example demonstrates:
 * - Ethernet initialization
 * - BlynkTaskStart() to spawn the Blynk task
 * - BLYNK_CONNECTED() handler with static guard to avoid timer slot leaks
 * - BLYNK_WRITE() handler to receive virtual pin data from Blynk app
 * - Timer callback (setInterval) to send data periodically
 * - Parallel task (HTTP) calling Blynk API without locks
 *
 * Hardware: ESP32 with Ethernet support
 * Blynk Server: Legacy Blynk server (e.g., blynk.cloud.blynk.io:8080)
 *
 * Build and run:
 *   idf.py build
 *   idf.py flash monitor
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "Ethernet.h"
#include "BlynkEspIDF.h"

// ============================================================================
// Configuration
// ============================================================================

// Replace with your actual Blynk authentication token
#define BLYNK_AUTH_TOKEN "YourAuthToken"

// Replace with your Blynk server address
#define BLYNK_SERVER "blynk.server.com"

// Blynk server port (typically 8080 for HTTP, 443 for HTTPS)
#define BLYNK_PORT 8080

// Optional: enable debug logging to see Blynk protocol messages
// #define BLYNK_DEBUG
#define BLYNK_PRINT stdout

// Virtual pin assignments
#define V_SENSOR_DATA 1  // Receive sensor commands from app
#define V_SEND_STATUS 2  // Send status updates to app
#define V_HTTP_STATUS 7  // Display HTTP response status code

// Logging tag
static const char *TAG = "BlynkApp";

// Timer instance (shared between main and Blynk task)
BlynkTimer timer;

// ============================================================================
// Virtual Pin Handlers (run inside Blynk task)
// ============================================================================

/**
 * BLYNK_WRITE(V1) - Handle data from Blynk app on virtual pin V1.
 *
 * This handler is called when the app sends a value to V1.
 * It runs inside the Blynk task, so all Blynk API calls are thread-safe.
 */
BLYNK_WRITE(V_SENSOR_DATA) {
    // Extract the integer value sent from the app
    int value = param.asInt();
    ESP_LOGI(TAG, "Received from app (V%d): %d", V_SENSOR_DATA, value);

    // Send acknowledgment back to app
    // This is thread-safe — no locks needed!
    Blynk.virtualWrite(V_SEND_STATUS, "Got your value!");
}

/**
 * BLYNK_CONNECTED() - Called every time device connects/reconnects to server.
 *
 * IMPORTANT: This fires on every reconnect, so use a static guard variable
 * to avoid registering duplicate timers (max 16 slots).
 */
BLYNK_CONNECTED() {
    ESP_LOGI(TAG, "Connected to Blynk server");

    // Static guard: only initialize timers once
    static bool timers_initialized = false;
    if (!timers_initialized) {
        // Register periodic callback to send heartbeat
        timer.setInterval(5000, []() {
            Blynk.virtualWrite(V_SEND_STATUS, "Online");
        });
        timers_initialized = true;
        ESP_LOGI(TAG, "Timers registered");
    }

    // Fetch latest values from app (optional)
    Blynk.syncVirtual(V_SENSOR_DATA);

    // Update UI property (optional)
    Blynk.setProperty(V_SEND_STATUS, "label", "Status");
}

/**
 * BLYNK_DISCONNECTED() - Called when connection to server is lost.
 */
BLYNK_DISCONNECTED() {
    ESP_LOGW(TAG, "Disconnected from Blynk server");
}

// ============================================================================
// Timer Callback (runs inside Blynk task)
// ============================================================================

/**
 * send_heartbeat() - Periodically inform the app that device is alive.
 *
 * Registered with timer.setInterval() before BlynkTaskStart().
 * This callback runs inside the Blynk task, so Blynk API calls are safe.
 */
static void send_heartbeat() {
    static int count = 0;
    count++;
    ESP_LOGI(TAG, "Heartbeat #%d", count);
    // Note: virtualWrite is called here without any locks!
    // (Internal mutex in BlynkProtocol handles thread safety)
    Blynk.virtualWrite(V_SEND_STATUS, count);
}

// ============================================================================
// Parallel Task: HTTP Example (runs on core 1)
// ============================================================================

/**
 * http_example_task() - Fetch data from an external HTTP service in parallel.
 *
 * This task runs on core 1 (APP_CPU) to avoid blocking the Blynk task.
 * Demonstrates: calling Blynk API from a different task is thread-safe!
 *
 * In a real application, this might:
 * - Fetch weather data from a REST API
 * - Poll a sensor via HTTP gateway
 * - Sync data with a backend service
 */
static void http_example_task(void *arg) {
    // Wait for Blynk to establish connection before making HTTP requests
    vTaskDelay(pdMS_TO_TICKS(3000));

    // HTTP client configuration
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",  // Echo service for demo
        .timeout_ms = 8000,
    };

    // Main loop: fetch every 15 seconds
    for (;;) {
        ESP_LOGI(TAG, "HTTP task: fetching data...");

        // Initialize HTTP client
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to create HTTP client");
            vTaskDelay(pdMS_TO_TICKS(15000));
            continue;
        }

        // Perform HTTP request
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP Status: %d", status);

            // *** IMPORTANT: Call Blynk API from this task ***
            // No locks needed! This is thread-safe due to internal mutex.
            Blynk.virtualWrite(V_HTTP_STATUS, status);
        } else {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
            // Optionally notify app of failure
            Blynk.virtualWrite(V_HTTP_STATUS, -1);
        }

        // Cleanup
        esp_http_client_cleanup(client);

        // Wait 15 seconds before next request
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

// ============================================================================
// App Entry Point
// ============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "App starting...");

    // Step 1: Initialize Ethernet
    // (Must be done before Blynk operations)
    ESP_LOGI(TAG, "Initializing Ethernet...");
    Eth.begin();

    // Step 2: Register timer callback
    // (Best to do this BEFORE BlynkTaskStart to avoid race conditions)
    ESP_LOGI(TAG, "Registering timer callbacks...");
    timer.setInterval(5000, send_heartbeat);

    // Step 3: Start Blynk task
    // Parameters:
    //   - auth token
    //   - server hostname
    //   - server port
    //   - timer instance (optional, nullptr if no timers)
    //   - stack size in bytes (8 KB is typical minimum)
    //   - task priority (5 is recommended)
    //   - core affinity (0 = PRO_CPU, 1 = APP_CPU)
    ESP_LOGI(TAG, "Starting Blynk task...");
    TaskHandle_t blynk_task = BlynkTaskStart(
        BLYNK_AUTH_TOKEN,
        BLYNK_SERVER,
        BLYNK_PORT,
        &timer,      // Pass timer for setInterval callbacks
        8192,        // Stack: 8 KB (increase if you hit stack overflow)
        5,           // Priority: 5 (leave at default)
        0            // Core: 0 (PRO_CPU, where network operations happen)
    );

    if (!blynk_task) {
        ESP_LOGE(TAG, "Failed to start Blynk task!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Blynk task started successfully");

    // Step 4: (Optional) Spawn additional tasks for CPU-intensive work
    // This task runs on core 1 to avoid blocking Blynk's network operations
    ESP_LOGI(TAG, "Spawning HTTP task on core 1...");
    xTaskCreatePinnedToCore(
        http_example_task,      // Task function
        "http_task",            // Task name (for debugging)
        8192,                   // Stack size
        NULL,                   // Parameter
        4,                      // Priority (lower than Blynk, which is 5)
        NULL,                   // Handle (optional)
        1                       // Core: 1 (APP_CPU)
    );

    // Step 5: Delete app_main task
    // The app is now driven by Blynk task (+ any spawned tasks).
    // app_main doesn't need to do anything else.
    ESP_LOGI(TAG, "app_main completed; Blynk task is running independently");
    vTaskDelete(NULL);
}

// ============================================================================
// Summary
// ============================================================================
//
// This example shows:
//
// 1. Basic Setup:
//    - Ethernet initialization
//    - BlynkTaskStart() spawns task that auto-connects and runs protocol
//
// 2. Event Handlers:
//    - BLYNK_CONNECTED(): guard timers with static bool
//    - BLYNK_WRITE(): handle incoming values from app
//    - BLYNK_DISCONNECTED(): optional cleanup
//
// 3. Timer Callbacks:
//    - Register before BlynkTaskStart() for simplicity
//    - Or use static guard in BLYNK_CONNECTED() if needed
//
// 4. Parallel Tasks:
//    - HTTP task on core 1 (non-blocking Blynk operations)
//    - Calls Blynk.virtualWrite() directly — no locks!
//    - Internal recursive mutex handles thread safety
//
// 5. Key Patterns:
//    - Do NOT call Blynk.run() manually — BlynkTaskStart() does it
//    - Do NOT call Blynk API from ISR — use FreeRTOS queue instead
//    - Do NOT register duplicate timers — use static guard in BLYNK_CONNECTED()
//    - DO call Blynk API from regular tasks freely — it's thread-safe!
//
// ============================================================================
