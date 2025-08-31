idf_component_register(
    SRCS 
        "Ethernet.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
	log
	lwip
	esp_netif
	esp_eth
	esp_hw_support
	esp_common
	esp_event
	freertos
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)