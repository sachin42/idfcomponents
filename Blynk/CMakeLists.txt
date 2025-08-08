idf_component_register(
    SRCS 
        "blynk-lib/utility/BlynkDebug.cpp"
        "blynk-lib/utility/utility.cpp"
        "blynk-lib/utility/BlynkHandlers.cpp"
        "blynk-lib/utility/BlynkTimer.cpp"
        "BlynkEspIDF.cpp"
        
        
    INCLUDE_DIRS 
        "include"
        "blynk-lib"
        "blynk-lib/utility"
        "blynk-lib/Blynk"

    REQUIRES lwip freertos esp_timer esp_rom Print
)
