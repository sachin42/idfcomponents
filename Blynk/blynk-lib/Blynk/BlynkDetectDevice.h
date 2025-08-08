/**
 * @file       BlynkDetectDevice.h
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       May 2016
 * @brief
 *
 */

#ifndef BlynkDetectDevice_h
#define BlynkDetectDevice_h

// General defines

#define BLYNK_NEWLINE "\r\n"

#define BLYNK_CONCAT(a, b) a ## b
#define BLYNK_CONCAT2(a, b) BLYNK_CONCAT(a, b)

#define BLYNK_STRINGIFY(x) #x
#define BLYNK_TOSTRING(x) BLYNK_STRINGIFY(x)

#define BLYNK_COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define BLYNK_ATTR_PACKED __attribute__ ((__packed__))
#define BLYNK_NORETURN __attribute__ ((noreturn))
#define BLYNK_UNUSED __attribute__((__unused__))
#define BLYNK_DEPRECATED __attribute__ ((deprecated))
#define BLYNK_CONSTRUCTOR __attribute__((constructor))

// Causes problems on some platforms
#define BLYNK_FORCE_INLINE inline //__attribute__((always_inline))

#ifndef BLYNK_INFO_DEVICE

    #define BLYNK_INFO_DEVICE  "ESP-IDF"\
        " (" BLYNK_TOSTRING(CONFIG_IDF_TARGET) " - " BLYNK_TOSTRING(CONFIG_IDF_INIT_VERSION) ")" 

    

    #if !defined(BLYNK_MAX_READBYTES) && defined(BLYNK_BUFFERS_SIZE)
    #define BLYNK_MAX_READBYTES  BLYNK_BUFFERS_SIZE
    #endif

    #if !defined(BLYNK_MAX_SENDBYTES) && defined(BLYNK_BUFFERS_SIZE)
    #define BLYNK_MAX_SENDBYTES  BLYNK_BUFFERS_SIZE
    #endif

    // Print diagnostics

    #if defined(BLYNK_DEBUG_ALL)
        #if defined(BLYNK_INFO_DEVICE)
        #pragma message ("BLYNK_INFO_DEVICE=" BLYNK_TOSTRING(BLYNK_INFO_DEVICE))
        #endif

        #if defined(BLYNK_INFO_CPU)
        #pragma message ("BLYNK_INFO_CPU="    BLYNK_TOSTRING(BLYNK_INFO_CPU))
        #endif

        #if defined(BLYNK_BUFFERS_SIZE)
        #pragma message ("BLYNK_BUFFERS_SIZE=" BLYNK_TOSTRING(BLYNK_BUFFERS_SIZE))
        #endif
    #endif

#endif

#endif
