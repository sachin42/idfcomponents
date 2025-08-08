set(srcs "Stream.cpp"
                    )

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES esp_system Print esp_timer WString
                    )
