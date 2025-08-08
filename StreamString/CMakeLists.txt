set(srcs "StreamString.cpp")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS include
                    REQUIRES WString Stream
                    )
