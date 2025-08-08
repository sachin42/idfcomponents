set(srcs "Print.cpp")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}
                    REQUIRES wstring
                    )
