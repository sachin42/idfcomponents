#pragma once

#include <map>
#include <functional>
#include "WString.h"
#include "mqtt_client.h"
#include "cJSON.h"

class MqttClient
{
public:
    /**
     * @brief Construct a new MqttClient instance
     */
    MqttClient();

    /**
     * @brief Destroy the MqttClient instance and release resources
     */
    ~MqttClient();

    /**
     * @brief Start the MQTT client using default broker URI and port from configuration
     *
     * @return ESP_OK on success
     *         ESP_FAIL if client initialization fails
     */
    esp_err_t start();

    /**
     * @brief Start the MQTT client with a custom broker URI and port
     *
     * @param brokerUri         MQTT broker URI (e.g., mqtts://broker.example.com)
     * @param brokerPort        MQTT broker port number
     * @return ESP_OK on success
     *         ESP_FAIL if client initialization fails
     */
    esp_err_t start(const String &brokerUri, int brokerPort);

    /**
     * @brief Publish a message to a specific topic
     *
     * @param topic             MQTT topic string
     * @param payload           Message payload
     * @param qos               Quality of Service level (default 1)
     * @param retain            Retain flag (default 0)
     * @return Message ID of the publish on success
     *         -1 if publishing failed
     */
    int publish(const String &topic, const String &payload, int qos = 1, int retain = 0);

    /**
     * @brief Subscribe to a specific MQTT topic
     *
     * @param topic             MQTT topic to subscribe to
     * @param qos               Quality of Service level (default 1)
     * @return Message ID of the subscribe on success
     *         -1 if subscription failed
     */
    int subscribe(const String &topic, int qos = 1);

    /**
     * @brief Unsubscribe from a topic
     *
     * @param topic             MQTT topic to unsubscribe from
     * @return Message ID of the unsubscribe on success
     *         -1 if unsubscribe failed
     */
    int unsubscribe(const String &topic);

    /**
     * @brief Stop the MQTT client and free associated resources
     */
    void stop();

    /**
     * @brief Check if the client is currently connected to the MQTT broker
     *
     * @return true if connected
     *         false if not connected
     */
    bool isConnected();

    /**
     * @brief Reconnect the MQTT client to the broker
     *
     * @return ESP_OK on success
     *         ESP_FAIL on failure
     */
    esp_err_t reconnect();

    /**
     * @brief Disconnect the MQTT client from the broker
     *
     * @return ESP_OK on success
     *         ESP_FAIL on failure
     */
    esp_err_t disconnect();

    /**
     * @brief Register a string payload callback for a specific topic
     *
     * @param topic             MQTT topic string
     * @param callback          Callback function accepting a payload as String
     * @param qos               Quality of Service level (default 1)
     * @param oneShot           If true, the callback is unregistered after first invocation
     */
    void registerCallback(const String &topic, std::function<void(const String &payload)> callback, int qos = 1, bool oneShot = false);

    /**
     * @brief Register a JSON callback for a specific topic without key validation
     *
     * @param topic             MQTT topic string
     * @param callback          Callback function accepting a cJSON pointer
     * @param qos               Quality of Service level (default 1)
     * @param oneShot           If true, the callback is unregistered after first invocation
     */
    void registerJsonCallback(const String &topic, std::function<void(cJSON *json)> callback, int qos = 1, bool oneShot = false);

    /**
     * @brief Register a JSON callback for a specific topic with required key validation
     *
     * @param topic             MQTT topic string
     * @param callback          Callback function accepting a cJSON pointer
     * @param requiredKeys      List of required JSON keys to validate before invoking callback
     * @param qos               Quality of Service level (default 1)
     * @param oneShot           If true, the callback is unregistered after first invocation
     */
    void registerJsonCallback(const String &topic, std::function<void(cJSON *json)> callback, const std::vector<String> &requiredKeys, int qos = 1, bool oneShot = false);

    /**
     * @brief Unregister a previously registered callback for a given topic
     *
     * @param topic             MQTT topic to remove from callback registry
     */
    void unregisterCallback(const String &topic);

private:
    /**
     * @brief Internal MQTT event dispatcher (called by MQTT library)
     *
     * @param handler_args      User context passed during registration (pointer to `this`)
     * @param base              Event base (should be MQTT base)
     * @param event_id          Type of MQTT event
     * @param event_data        Pointer to esp_mqtt_event_handle_t containing event details
     */
    static void eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

    /**
     * @brief Handle MQTT event and dispatch to appropriate topic callback
     *
     * @param event             MQTT event handle containing topic, payload, and metadata
     */
    void handleEvent(esp_mqtt_event_handle_t event);

    bool connected = false; //
    esp_mqtt_client_handle_t client;
    struct CallbackEntry
    {
        int qos;
        std::function<void(const String &payload)> strCallback;
        std::function<void(cJSON *json)> jsonCallback;
        std::vector<String> requiredKeys; // for JSON validation
        bool oneShot = false;
    };
    std::map<String, CallbackEntry> topicCallbacks;

    String brokerUri = CONFIG_MQTT_BROKER;
    int brokerPort = CONFIG_MQTT_PORT;
#ifdef CONFIG_MQTT_CLIENT_ID
    String clientId = CONFIG_MQTT_CLIENT_ID;
#endif
#if CONFIG_USE_MQTT_CLIENT_AUTH
    String username = CONFIG_MQTT_USERNAME;
    String password = CONFIG_MQTT_PASSWORD;
#endif
};

extern MqttClient mqtt;
