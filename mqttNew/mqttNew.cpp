#include "mqttNew.hpp"
#include "esp_log.h"
#include "freertos/task.h"
#include <cstring>

static const char *TAG = "MQTT";

MqttClient mqtt;

MqttClient::MqttClient() : client(nullptr) {}

MqttClient::~MqttClient()
{
    stop();
}

esp_err_t MqttClient::start()
{
    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.broker.address.uri = brokerUri.c_str();
    // mqtt_cfg.broker.address.uri = "mqtt://192.168.1.14";
    mqtt_cfg.broker.address.port = brokerPort;
    mqtt_cfg.task.stack_size = 4096;

#ifdef CONFIG_MQTT_CLIENT_ID
    mqtt_cfg.credentials.client_id = clientId.c_str();
#endif

#if CONFIG_USE_MQTT_CLIENT_AUTH
    mqtt_cfg.credentials.username = username.c_str();
    mqtt_cfg.credentials.authentication.password = password.c_str();
#endif

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT init failed");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, MqttClient::eventHandler, this));
    return esp_mqtt_client_start(client);
}

esp_err_t MqttClient::start(const String &brokerUri, int brokerPort)
{
    this->brokerUri = brokerUri;
    this->brokerPort = brokerPort;
    return start();
}

esp_err_t MqttClient::reconnect()
{
    if (!client)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return ESP_FAIL;
    }
    return esp_mqtt_client_reconnect(client);
}

esp_err_t MqttClient::disconnect()
{
    if (!client)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return ESP_FAIL;
    }
    return esp_mqtt_client_disconnect(client);
}

bool MqttClient::isConnected()
{
    return connected;
}

int MqttClient::publish(const String &topic, const String &payload, int qos, int retain)
{
    if (!client)
        return ESP_FAIL;
    return esp_mqtt_client_publish(client, topic.c_str(), payload.c_str(), payload.length(), qos, retain);
}

int MqttClient::subscribe(const String &topic, int qos)
{
    if (!client)
        return ESP_FAIL;
    return esp_mqtt_client_subscribe(client, topic.c_str(), qos);
}

int MqttClient::unsubscribe(const String &topic)
{
    if (!client)
        return ESP_FAIL;
    return esp_mqtt_client_unsubscribe(client, topic.c_str());
}

void MqttClient::registerCallback(const String &topic, std::function<void(const String &payload)> callback, int qos, bool oneShot)
{
    topicCallbacks[topic] = {qos, callback, nullptr, {}, oneShot};
    if (connected)
    {
        subscribe(topic, qos);
        ESP_LOGD(TAG, "Subscribed (late) to topic: %s", topic.c_str());
    }
}

void MqttClient::registerJsonCallback(const String &topic, std::function<void(cJSON *json)> callback, int qos, bool oneShot)
{
    topicCallbacks[topic] = {qos, nullptr, callback, {}, oneShot};
    if (connected)
    {
        subscribe(topic, qos);
        ESP_LOGD(TAG, "Subscribed (late) to topic: %s", topic.c_str());
    }
}

void MqttClient::registerJsonCallback(const String &topic, std::function<void(cJSON *json)> callback, const std::vector<String> &requiredKeys, int qos, bool oneShot)
{
    topicCallbacks[topic] = {qos, nullptr, callback, requiredKeys, oneShot};
    if (connected)
    {
        subscribe(topic, qos);
        ESP_LOGD(TAG, "Subscribed (late) to topic: %s", topic.c_str());
    }
}

void MqttClient::unregisterCallback(const String &topic)
{
    auto it = topicCallbacks.find(topic);
    if (it != topicCallbacks.end())
    {
        topicCallbacks.erase(it);
        if (connected)
        {
            unsubscribe(topic);
            ESP_LOGD(TAG, "Unsubscribed from topic: %s", topic.c_str());
        }
    }
}

void MqttClient::stop()
{
    if (client)
    {
        esp_mqtt_client_unregister_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, MqttClient::eventHandler);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
}

void MqttClient::eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    auto *self = static_cast<MqttClient *>(handler_args);
    if (self)
    {
        self->handleEvent(static_cast<esp_mqtt_event_handle_t>(event_data));
    }
}

void MqttClient::handleEvent(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        connected = true;
        ESP_LOGD(TAG, "Connected to broker");
        for (const auto &[topic, entry] : topicCallbacks)
        {
            subscribe(topic, entry.qos); // subscribe to all registered
            ESP_LOGD(TAG, "Subscribed to topic: %s", topic.c_str());
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        connected = false;
        ESP_LOGW(TAG, "Disconnected from broker");
        break;
    case MQTT_EVENT_DATA:
    {
        String topic(event->topic, event->topic_len);
        String payload(event->data, event->data_len);

        ESP_LOGD(TAG, "Received on [%s]: %s", topic.c_str(), payload.c_str());

        // Call registered callback
        auto it = topicCallbacks.find(topic);
        if (it != topicCallbacks.end())
        {
            auto entry = it->second;
            if (entry.strCallback)
                entry.strCallback(payload);
            else if (entry.jsonCallback)
            {
                cJSON *root = cJSON_Parse(payload.c_str());
                if (!root)
                    ESP_LOGE(TAG, "Invalid JSON on topic: %s", topic.c_str());
                else
                {
                    bool valid = true;
                    if (!entry.requiredKeys.empty())
                    {
                        for (const auto &key : entry.requiredKeys)
                        {
                            if (!cJSON_HasObjectItem(root, key.c_str()))
                            {
                                ESP_LOGE(TAG, "JSON missing required key '%s' on topic: %s", key.c_str(), topic.c_str());
                                valid = false;
                                break;
                            }
                        }
                    }

                    if (valid)
                    {
                        entry.jsonCallback(root);
                    }

                    cJSON_Delete(root);
                }
            }

            // Handle one-shot
            if (entry.oneShot)
            {
                unregisterCallback(topic);
            }
        }
        else
        {
            ESP_LOGW(TAG, "No callback registered for topic: %s", topic.c_str());
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        if (event->error_handle)
        {
            ESP_LOGE(TAG, "MQTT Error: 0x%x", event->error_handle->esp_tls_last_esp_err);
        }
        break;

    default:
        break;
    }
}
