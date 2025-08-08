#ifndef HARDWARESERIAL_H
#define HARDWARESERIAL_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Stream.h" 
#include "WString.h"
#define SERIAL_5N1 0x00
#define SERIAL_6N1 0x02
#define SERIAL_7N1 0x04
#define SERIAL_8N1 0x06
#define SERIAL_5N2 0x08
#define SERIAL_6N2 0x0A
#define SERIAL_7N2 0x0C
#define SERIAL_8N2 0x0E
#define SERIAL_5E1 0x20
#define SERIAL_6E1 0x22
#define SERIAL_7E1 0x24
#define SERIAL_8E1 0x26
#define SERIAL_5E2 0x28
#define SERIAL_6E2 0x2A
#define SERIAL_7E2 0x2C
#define SERIAL_8E2 0x2E
#define SERIAL_5O1 0x30
#define SERIAL_6O1 0x32
#define SERIAL_7O1 0x34
#define SERIAL_8O1 0x36
#define SERIAL_5O2 0x38
#define SERIAL_6O2 0x3A
#define SERIAL_7O2 0x3C
#define SERIAL_8O2 0x3E

class HardwareSerial : public Stream {
private:
    uart_port_t _uart_num;
    int _tx_pin;
    int _rx_pin;
    int _rts_pin;
    int _cts_pin;
    uint32_t _baud;
    uint32_t _config;
    bool _initialized;
    int _rx_buffer_size;
    int _tx_buffer_size;
    static const char* TAG;
    uint8_t _peek_buffer;
    bool _peek_buffer_valid;
    
    void _updateConfig();
    uart_word_length_t _getDataBits(uint32_t config);
    uart_stop_bits_t _getStopBits(uint32_t config);
    uart_parity_t _getParity(uint32_t config);

public:
    HardwareSerial(uart_port_t uart_num);
    ~HardwareSerial();
    
    // Arduino-compatible methods
    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
    void beginRxOnly(unsigned long baud, int8_t rxPin, uint32_t config = SERIAL_8N1, bool invert = false);
    void end();
    
    // Stream interface implementation
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    
    // Additional Arduino Serial methods
    size_t readBytes(char *buffer, size_t length);
    size_t readBytes(uint8_t *buffer, size_t length);
    String readString();
    String readStringUntil(char terminator);
    int readBytesUntil(char terminator, char *buffer, size_t length);
    int readBytesUntil(char terminator, uint8_t *buffer, size_t length);
    
    // Configuration methods
    void setBaudRate(uint32_t baud);
    uint32_t baudRate();
    void setDebugOutput(bool);
    
    // ESP32 specific methods
    void setRxBufferSize(size_t size);
    void setTxBufferSize(size_t size);
    void setHwFlowCtrlMode(uart_hw_flowcontrol_t mode, uint8_t threshold = 64);
    void setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin = -1, int8_t rtsPin = -1);
    void setRxInvert(bool invert);
    void setRxTimeout(uint8_t timeout);
    
    // Status methods
    bool isConnected();
    int availableForWrite();
    
    // Compatibility with Arduino examples
    operator bool() const;
    
    // Low level access
    uart_port_t getUartNum() { return _uart_num; }
};

// Global instances for compatibility
// extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;

#endif // HARDWARESERIAL_H