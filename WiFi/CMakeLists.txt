idf_component_register(
    SRCS 
        "WiFi.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
	log
	lwip
	esp_wifi
	esp_common
	esp_event
	wifi_provisioning
	freertos
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
