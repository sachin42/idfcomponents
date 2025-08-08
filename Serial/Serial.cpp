#include "Serial.h"
#include <cstring>
#include <algorithm>

const char* HardwareSerial::TAG = "HardwareSerial";

// Global instances
// HardwareSerial Serial(UART_NUM_0);
HardwareSerial Serial1(UART_NUM_1);
HardwareSerial Serial2(UART_NUM_2);

HardwareSerial::HardwareSerial(uart_port_t uart_num) : 
    _uart_num(uart_num),
    _tx_pin(-1),
    _rx_pin(-1),
    _rts_pin(-1),
    _cts_pin(-1),
    _baud(115200),
    _config(SERIAL_8N1),
    _initialized(false),
    _rx_buffer_size(256),
    _tx_buffer_size(256),
    _peek_buffer(0),
    _peek_buffer_valid(false)
{
}

HardwareSerial::~HardwareSerial() {
    end();
}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd) {
    if (_initialized) {
        end();
    }
    
    _baud = baud;
    _config = config;
    
    // Set default pins if not specified
    if (rxPin == -1 || txPin == -1) {
        switch (_uart_num) {
            case UART_NUM_0:
                _rx_pin = (rxPin == -1) ? 3 : rxPin;
                _tx_pin = (txPin == -1) ? 1 : txPin;
                break;
            case UART_NUM_1:
                _rx_pin = (rxPin == -1) ? 9 : rxPin;
                _tx_pin = (txPin == -1) ? 10 : txPin;
                break;
            case UART_NUM_2:
                _rx_pin = (rxPin == -1) ? 16 : rxPin;
                _tx_pin = (txPin == -1) ? 17 : txPin;
                break;
            default:
                ESP_LOGE(TAG, "Invalid UART number");
                return;
        }
    } else {
        _rx_pin = rxPin;
        _tx_pin = txPin;
    }
    
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = (int)_baud,
        .data_bits = _getDataBits(_config),
        .parity = _getParity(_config),
        .stop_bits = _getStopBits(_config),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = rxfifo_full_thrhd,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .backup_before_sleep = 0
        },
    };
    
    ESP_ERROR_CHECK(uart_param_config(_uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(_uart_num, _tx_pin, _rx_pin, _rts_pin, _cts_pin));
    ESP_ERROR_CHECK(uart_driver_install(_uart_num, _rx_buffer_size, _tx_buffer_size, 10, NULL, 0));
    
    if (invert) {
        uart_set_line_inverse(_uart_num, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
    }
    
    _initialized = true;
    ESP_LOGD(TAG, "UART%d initialized: baud=%lu, rx=%d, tx=%d", _uart_num, _baud, _rx_pin, _tx_pin);
}

void HardwareSerial::beginRxOnly(unsigned long baud, int8_t rxPin, uint32_t config, bool invert) {
    if (_initialized) {
        end();
    }
    
    _baud = baud;
    _config = config;
    _rx_pin = rxPin;
    _tx_pin = UART_PIN_NO_CHANGE; // Disable TX pin
    _rts_pin = UART_PIN_NO_CHANGE;
    _cts_pin = UART_PIN_NO_CHANGE;
    
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = (int)_baud,
        .data_bits = _getDataBits(_config),
        .parity = _getParity(_config),
        .stop_bits = _getStopBits(_config),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 112,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .backup_before_sleep = 0
        },
    };
    
    ESP_ERROR_CHECK(uart_param_config(_uart_num, &uart_config));
    // Set only RX pin, TX disabled
    ESP_ERROR_CHECK(uart_set_pin(_uart_num, UART_PIN_NO_CHANGE, _rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Install driver with TX buffer = 0 for RX-only mode
    ESP_ERROR_CHECK(uart_driver_install(_uart_num, _rx_buffer_size, 0, 10, NULL, 0));
    
    if (invert) {
        uart_set_line_inverse(_uart_num, UART_SIGNAL_RXD_INV);
    }
    
    _initialized = true;
    ESP_LOGI(TAG, "UART%d initialized (RX-only): baud=%lu, rx=%d", _uart_num, _baud, _rx_pin);
}

void HardwareSerial::end() {
    if (_initialized) {
        uart_driver_delete(_uart_num);
        _initialized = false;

    }
}

int HardwareSerial::available() {
    if (!_initialized) return 0;
    
    size_t available_bytes = 0;
    uart_get_buffered_data_len(_uart_num, &available_bytes);
    return available_bytes + (_peek_buffer_valid ? 1 : 0);
}

int HardwareSerial::read() {
    if (!_initialized) return -1;
    
    if (_peek_buffer_valid) {
        _peek_buffer_valid = false;
        return _peek_buffer;
    }
    
    uint8_t byte;
    int len = uart_read_bytes(_uart_num, &byte, 1, 0);
    return (len > 0) ? byte : -1;
}

int HardwareSerial::peek() {
    if (!_initialized) return -1;
    
    if (_peek_buffer_valid) {
        return _peek_buffer;
    }
    
    uint8_t byte;
    int len = uart_read_bytes(_uart_num, &byte, 1, 0);
    if (len > 0) {
        _peek_buffer = byte;
        _peek_buffer_valid = true;
        return byte;
    }
    return -1;
}

void HardwareSerial::flush() {
    if (!_initialized) return;
    // Clear peek buffer
    _peek_buffer_valid = false;
    uart_wait_tx_done(_uart_num, portMAX_DELAY);
}

size_t HardwareSerial::write(uint8_t byte) {
    if (!_initialized) return 0;
    
    int written = uart_write_bytes(_uart_num, &byte, 1);
    return (written >= 0) ? written : 0;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
    if (!_initialized || !buffer) return 0;
    
    int written = uart_write_bytes(_uart_num, buffer, size);
    return (written >= 0) ? written : 0;
}

size_t HardwareSerial::readBytes(char *buffer, size_t length) {
    return readBytes((uint8_t*)buffer, length);
}

size_t HardwareSerial::readBytes(uint8_t *buffer, size_t length) {
    if (!_initialized || !buffer) return 0;
    
    int read_len = uart_read_bytes(_uart_num, buffer, length, pdMS_TO_TICKS(1000));
    return (read_len >= 0) ? read_len : 0;
}

String HardwareSerial::readString() {
    String result;
    if (!_initialized) return result;
    
    int c;
    while ((c = read()) >= 0) {
        result += (char)c;
        if (available() == 0) {
            vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to allow more data
            if (available() == 0) break;
        }
    }
    return result;
}

String HardwareSerial::readStringUntil(char terminator) {
    String result;
    if (!_initialized) return result;
    
    int c;
    while ((c = read()) >= 0) {
        if (c == terminator) break;
        result += (char)c;
    }
    return result;
}

int HardwareSerial::readBytesUntil(char terminator, char *buffer, size_t length) {
    return readBytesUntil(terminator, (uint8_t*)buffer, length);
}

int HardwareSerial::readBytesUntil(char terminator, uint8_t *buffer, size_t length) {
    if (!_initialized || !buffer) return 0;
    
    size_t index = 0;
    int c;
    while (index < length && (c = read()) >= 0) {
        if (c == terminator) break;
        buffer[index++] = c;
    }
    return index;
}

void HardwareSerial::setBaudRate(uint32_t baud) {
    if (!_initialized) return;
    
    _baud = baud;
    uart_set_baudrate(_uart_num, baud);
}

uint32_t HardwareSerial::baudRate() {
    uint32_t baud;
    uart_get_baudrate(_uart_num, &baud);
    return baud;
}

void HardwareSerial::setDebugOutput(bool enable) {
    // ESP-IDF equivalent would be setting log level
    if (enable) {
        esp_log_level_set(TAG, ESP_LOG_DEBUG);
    } else {
        esp_log_level_set(TAG, ESP_LOG_WARN);
    }
}

void HardwareSerial::setRxBufferSize(size_t size) {
    _rx_buffer_size = size;
    if (_initialized) {
        ESP_LOGW(TAG, "Buffer size change will take effect after next begin()");
    }
}

void HardwareSerial::setTxBufferSize(size_t size) {
    _tx_buffer_size = size;
    if (_initialized) {
        ESP_LOGW(TAG, "Buffer size change will take effect after next begin()");
    }
}

void HardwareSerial::setHwFlowCtrlMode(uart_hw_flowcontrol_t mode, uint8_t threshold) {
    if (!_initialized) return;
    uart_set_hw_flow_ctrl(_uart_num, mode, threshold);
}

void HardwareSerial::setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin) {
    _rx_pin = rxPin;
    _tx_pin = txPin;
    _cts_pin = ctsPin;
    _rts_pin = rtsPin;
    
    if (_initialized) {
        uart_set_pin(_uart_num, _tx_pin, _rx_pin, _rts_pin, _cts_pin);
    }
}

void HardwareSerial::setRxInvert(bool invert) {
    if (!_initialized) return;
    
    if (invert) {
        uart_set_line_inverse(_uart_num, UART_SIGNAL_RXD_INV);
    } else {
        uart_disable_intr_mask(_uart_num, UART_SIGNAL_RXD_INV);
    }
}

void HardwareSerial::setRxTimeout(uint8_t timeout) {
    if (!_initialized) return;
    uart_set_rx_timeout(_uart_num, timeout);
}

bool HardwareSerial::isConnected() {
    return _initialized;
}

int HardwareSerial::availableForWrite() {
    if (!_initialized) return 0;
    
    // ESP-IDF doesn't directly provide this, approximate with buffer size
    return _tx_buffer_size - 1; // Conservative estimate
}

HardwareSerial::operator bool() const {
    return _initialized;
}

// Private helper methods
uart_word_length_t HardwareSerial::_getDataBits(uint32_t config) {
    switch (config & 0x07) {
        case 0x00: return UART_DATA_5_BITS;
        case 0x02: return UART_DATA_6_BITS;
        case 0x04: return UART_DATA_7_BITS;
        case 0x06: return UART_DATA_8_BITS;
        default: return UART_DATA_8_BITS;
    }
}

uart_stop_bits_t HardwareSerial::_getStopBits(uint32_t config) {
    return (config & 0x08) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
}

uart_parity_t HardwareSerial::_getParity(uint32_t config) {
    if (config & 0x20) return UART_PARITY_EVEN;
    if (config & 0x30) return UART_PARITY_ODD;
    return UART_PARITY_DISABLE;
}