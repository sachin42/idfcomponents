idf_component_register(SRCS "OTAUpdater.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES "WString" "esp_http_client" "app_update" "mbedtls" "bootloader_support")

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)