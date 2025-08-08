idf_component_register(SRCS "BluetoothSerial.cpp" "BTAddress.cpp" "BTAdvertisedDeviceSet.cpp" "BTScanResultsSet.cpp" "esp32-hal-bt.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt Stream Print esp_system log WString freertos)