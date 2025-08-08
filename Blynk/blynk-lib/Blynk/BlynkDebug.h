/**
 * @file       BlynkDebug.h
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Jan 2015
 * @brief      Debug utilities
 *
 */

#ifndef BlynkDebug_h
#define BlynkDebug_h

#include <Blynk/BlynkConfig.h>
#include <stddef.h>
#include <inttypes.h>
#include "sdkconfig.h"

typedef uint32_t millis_time_t;

void            BlynkDelay(millis_time_t ms);
millis_time_t   BlynkMillis();
size_t          BlynkFreeRam();
void            BlynkReset() BLYNK_NORETURN;
void            BlynkFatal() BLYNK_NORETURN;


#if !defined(BLYNK_RUN_YIELD)
    #if defined(BLYNK_NO_YIELD)
        #define BLYNK_RUN_YIELD() {}
    #elif defined(SPARK) || defined(PARTICLE)
        #define BLYNK_RUN_YIELD() { Particle.process(); }
    #elif !defined(ARDUINO) || (ARDUINO < 151)
        #define BLYNK_RUN_YIELD() {}
    #else
        #define BLYNK_RUN_YIELD() { BlynkDelay(0); }
    #endif
#endif

#define BLYNK_PROGMEM
#define BLYNK_F(s) s
#define BLYNK_PSTR(s) s

#if defined(BLYNK_DEBUG_ALL) && !(__cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__))
    #warning "Compiler features not enabled -> please contact yor board vendor to enable c++0x"
#endif

// Diagnostic defines

#define BLYNK_FATAL(msg)     { BLYNK_LOG1(msg); BlynkFatal(); }
#define BLYNK_LOG_RAM()      { BLYNK_LOG2(BLYNK_F("Free RAM: "), BlynkFreeRam()); }
#define BLYNK_LOG_FN()       BLYNK_LOG3(BLYNK_F(__FUNCTION__), '@', __LINE__);
#define BLYNK_LOG_TROUBLE(t) BLYNK_LOG2(BLYNK_F("Trouble detected: http://docs.blynk.cc/#troubleshooting-"), t)

#ifndef BLYNK_PRINT
#undef BLYNK_DEBUG
#endif

#ifdef BLYNK_DEBUG_ALL
#define BLYNK_DEBUG
#endif

#ifdef BLYNK_PRINT

    #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)

        #include <assert.h>
        #include <stdio.h>
        #include <string.h>
        #include <errno.h>
        #include <signal.h>

        #include <iostream>
        using namespace std;
        #define BLYNK_LOG(msg, ...)       { fprintf(BLYNK_PRINT, "[%ld] " msg BLYNK_NEWLINE, BlynkMillis(), ##__VA_ARGS__); }
        #define BLYNK_LOG1(p1)            { BLYNK_LOG_TIME(); cout << p1 << endl; }
        #define BLYNK_LOG2(p1,p2)         { BLYNK_LOG_TIME(); cout << p1 << p2 << endl; }
        #define BLYNK_LOG3(p1,p2,p3)      { BLYNK_LOG_TIME(); cout << p1 << p2 << p3 << endl; }
        #define BLYNK_LOG4(p1,p2,p3,p4)   { BLYNK_LOG_TIME(); cout << p1 << p2 << p3 << p4 << endl; }
        #define BLYNK_LOG6(p1,p2,p3,p4,p5,p6)   { BLYNK_LOG_TIME(); cout << p1 << p2 << p3 << p4 << p5 << p6 << endl; }

        #define BLYNK_LOG_TIME() cout << '[' << BlynkMillis() << "] ";

#ifdef BLYNK_DEBUG
        #define BLYNK_DBG_BREAK()    raise(SIGTRAP);
        #define BLYNK_ASSERT(expr)   assert(expr)

        static
        void BLYNK_DBG_DUMP(const char* msg, const void* addr, size_t len) {
            BLYNK_LOG_TIME();
            fprintf(BLYNK_PRINT, "%s", msg);
            int l2 = len;
            const uint8_t* octets = (const uint8_t*)addr;
            bool prev_print = true;
            while (l2--) {
                const uint8_t c = *octets++ & 0xFF;
                if (c >= 32 && c < 127) {
                    if (!prev_print) { fputc(']', BLYNK_PRINT); }
                    fputc((char)c, BLYNK_PRINT);
                    prev_print = true;
                } else {
                    fputc(prev_print?'[':'|', BLYNK_PRINT);
                    fprintf(BLYNK_PRINT, "%02x", c);
                    prev_print = false;
                }
            }
            fprintf(BLYNK_PRINT, "%s" BLYNK_NEWLINE, prev_print?"":"]");
        }
#endif

    #else

        #warning "Cannot detect platform"

    #endif

#endif

#ifndef BLYNK_LOG
    #define BLYNK_LOG(...)
    #define BLYNK_LOG1(p1)
    #define BLYNK_LOG2(p1,p2)
    #define BLYNK_LOG3(p1,p2,p3)
    #define BLYNK_LOG4(p1,p2,p3,p4)
    #define BLYNK_LOG6(p1,p2,p3,p4,p5,p6)
    #define BLYNK_LOG_IP(msg, ip)
    #define BLYNK_LOG_IP_REV(msg, ip)
#endif

#ifndef BLYNK_DBG_BREAK
    #define BLYNK_DBG_BREAK()
    #define BLYNK_ASSERT(expr)
    #define BLYNK_DBG_DUMP(msg, addr, len)
#endif

#endif
