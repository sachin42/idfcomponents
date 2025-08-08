idf_build_get_property(target IDF_TARGET)

set(srcs "stdlib_noniso.c"
         "WString.cpp")


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES esp_system
                    )
