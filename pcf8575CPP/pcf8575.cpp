#include "pcf8575.h"
#include <cstring>
#include <esp_err.h>
#include <esp_idf_lib_helpers.h>

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

PCF8575::PCF8575() {
    memset(&dev, 0, sizeof(i2c_dev_t));
}

PCF8575::~PCF8575() {
    end();
}

esp_err_t PCF8575::begin(uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    CHECK_ARG(addr >= PCF8575_I2C_ADDR_BASE && addr <= (PCF8575_I2C_ADDR_BASE + 7));

    dev.port = port;
    dev.addr = addr;
    dev.cfg.sda_io_num = sda_gpio;
    dev.cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev.cfg.master.clk_speed = I2C_FREQ_HZ;
#endif

    return i2c_dev_create_mutex(&dev);
}

esp_err_t PCF8575::end() {
    if (dev.mutex) {
        esp_err_t ret = i2c_dev_delete_mutex(&dev);
        memset(&dev, 0, sizeof(i2c_dev_t));
        return ret;
    }
    return ESP_OK;
}

esp_err_t PCF8575::readPort(uint16_t *val) {
    CHECK_ARG(val);
    CHECK_ARG(dev.mutex);

    I2C_DEV_TAKE_MUTEX(&dev);
    I2C_DEV_CHECK(&dev, i2c_dev_read(&dev, NULL, 0, val, 2));
    I2C_DEV_GIVE_MUTEX(&dev);

    return ESP_OK;
}

esp_err_t PCF8575::writePort(uint16_t val) {
    CHECK_ARG(dev.mutex);

    I2C_DEV_TAKE_MUTEX(&dev);
    I2C_DEV_CHECK(&dev, i2c_dev_write(&dev, NULL, 0, &val, 2));
    I2C_DEV_GIVE_MUTEX(&dev);

    return ESP_OK;
}

esp_err_t PCF8575::portRead(uint16_t *val) {
    return readPort(val);
}

esp_err_t PCF8575::portWrite(uint16_t value) {
    return writePort(value);
}

bool PCF8575::digitalRead(uint8_t pinnum) {
    if (pinnum > 15 || !dev.mutex) {
        return false;
    }

    uint16_t val;
    if (readPort(&val) != ESP_OK) {
        return false;
    }
    
    return (val >> pinnum) & 0x1;
}

esp_err_t PCF8575::digitalWrite(uint8_t pinnum, bool val) {
    CHECK_ARG(pinnum <= 15);
    CHECK_ARG(dev.mutex);

    uint16_t portValue;
    CHECK(readPort(&portValue));
    
    if (val) {
        portValue |= (1 << pinnum);
    } else {
        portValue &= ~(1 << pinnum);
    }
    
    return writePort(portValue);
}

esp_err_t PCF8575::pinMode(uint8_t pinnum, PinMode mode) {
    CHECK_ARG(pinnum <= 15);
    CHECK_ARG(dev.mutex);

    uint16_t portValue;
    CHECK(readPort(&portValue));
    
    if (mode == INPUT || mode == INPUT_PULLUP) {
        portValue |= (1 << pinnum);  // Set pin high for input mode
    } else {
        portValue &= ~(1 << pinnum); // Set pin low for output mode
    }
    
    return writePort(portValue);
}

esp_err_t PCF8575::writeMultiplePins(uint16_t mask, uint16_t values) {
    CHECK_ARG(dev.mutex);

    uint16_t portValue;
    CHECK(readPort(&portValue));
    
    // Clear the bits we want to modify
    portValue &= ~mask;
    // Set the new values only for the masked bits
    portValue |= (values & mask);
    
    return writePort(portValue);
}

uint16_t PCF8575::readMultiplePins(uint16_t mask) {
    if (!dev.mutex) {
        return 0;
    }

    uint16_t val;
    if (readPort(&val) != ESP_OK) {
        return 0;
    }
    
    return val & mask;
}

bool PCF8575::isInitialized() const {
    return dev.mutex != NULL;
}