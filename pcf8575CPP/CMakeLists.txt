idf_component_register(
    SRCS pcf8575.cpp
    INCLUDE_DIRS .
    REQUIRES i2cdev log esp_idf_lib_helpers
)
