/**
 * @file       WidgetTerminal.h
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Jan 2015
 * @brief
 */

#ifndef WidgetTerminal_h
#define WidgetTerminal_h

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TERMINAL_PRINT_BUF_SIZE 128

#include <Blynk/BlynkWidgetBase.h>
#include <Print.h>


class WidgetTerminal
    : public BlynkWidgetBase, public Print
{
public:
    WidgetTerminal(uint8_t vPin)
        : BlynkWidgetBase(vPin)
        , mOutQty(0)
    {}

    //virtual ~WidgetTerminal() {}

    virtual size_t write(uint8_t byte) {
        mOutBuf[mOutQty++] = byte;
        if (mOutQty >= sizeof(mOutBuf)) {
            flush();
        }
        return 1;
    }

    virtual void flush() {
        if (mOutQty) {
            Blynk.virtualWriteBinary(mPin, mOutBuf, mOutQty);
            mOutQty = 0;
        }
    }
    
    void clear() {
        flush();
        Blynk.virtualWrite(mPin, "clr");
    }

    using Print::write;

    virtual size_t write(const void* buff, size_t len) {
        return write((char*)buff, len);
    }

private:
    uint8_t mOutBuf[64];
    uint8_t mOutQty;
};

#endif
