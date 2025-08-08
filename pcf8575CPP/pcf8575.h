#ifndef __PCF8575_HPP__
#define __PCF8575_HPP__

#include <stddef.h>
#include <i2cdev.h>
#include <esp_err.h>
#include <driver/gpio.h>

#define PCF8575_I2C_ADDR_BASE 0x20

/**
 * @brief PCF8575 16-bit I/O expander driver class
 */
class PCF8575 {
public:
    /**
     * @brief Pin modes for PCF8575
     */
    enum PinMode {
        OUTPUT = 0,
        INPUT = 1,
        INPUT_PULLUP = 1
    };

private:
    i2c_dev_t dev;
    static const uint32_t I2C_FREQ_HZ = 400000;

    /**
     * @brief Internal function to read port value
     * @param val Pointer to store 16-bit port value
     * @return ESP_OK on success
     */
    esp_err_t readPort(uint16_t *val);

    /**
     * @brief Internal function to write port value
     * @param val 16-bit port value to write
     * @return ESP_OK on success
     */
    esp_err_t writePort(uint16_t val);

public:
    /**
     * @brief Constructor for PCF8575 class
     */
    PCF8575();

    /**
     * @brief Destructor for PCF8575 class
     */
    ~PCF8575();

    /**
     * @brief Initialize PCF8575 device
     * 
     * @param addr I2C address (0b0100<A2><A1><A0> for PCF8575)
     * @param port I2C port number
     * @param sda_gpio SDA GPIO pin
     * @param scl_gpio SCL GPIO pin
     * @return ESP_OK on success
     */
    esp_err_t begin(uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);

    /**
     * @brief Deinitialize PCF8575 device
     * @return ESP_OK on success
     */
    esp_err_t end();

    /**
     * @brief Read entire 16-bit GPIO port value
     * @param val Pointer to store 16-bit GPIO port value
     * @return ESP_OK on success
     */
    esp_err_t portRead(uint16_t *val);

    /**
     * @brief Write value to entire 16-bit GPIO port
     * @param value 16-bit GPIO port value
     * @return ESP_OK on success
     */
    esp_err_t portWrite(uint16_t value);

    /**
     * @brief Read individual pin value
     * @param pinnum Pin number (0-15)
     * @return true if pin is high, false if pin is low
     */
    bool digitalRead(uint8_t pinnum);

    /**
     * @brief Write value to individual pin
     * @param pinnum Pin number (0-15)
     * @param val Value to write (true for high, false for low)
     * @return ESP_OK on success
     */
    esp_err_t digitalWrite(uint8_t pinnum, bool val);

    /**
     * @brief Set pin mode for individual pin
     * @param pinnum Pin number (0-15)
     * @param mode Pin mode (OUTPUT, INPUT, INPUT_PULLUP)
     * @return ESP_OK on success
     */
    esp_err_t pinMode(uint8_t pinnum, PinMode mode);

    /**
     * @brief Set multiple pins as outputs and write values
     * @param mask Bit mask for pins to set (1 = modify pin, 0 = leave unchanged)
     * @param values Values for the pins (1 = high, 0 = low)
     * @return ESP_OK on success
     */
    esp_err_t writeMultiplePins(uint16_t mask, uint16_t values);

    /**
     * @brief Read multiple pins at once
     * @param mask Bit mask for pins to read
     * @return Masked pin values
     */
    uint16_t readMultiplePins(uint16_t mask);

    /**
     * @brief Check if device is properly initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;
};

#endif /* __PCF8575_HPP__ */
