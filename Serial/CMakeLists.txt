# CMakeLists.txt for ESP-IDF Serial Library Component

idf_component_register(
    SRCS 
        "Serial.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        driver
        esp_common
        freertos
        log
        Stream
        WString
)