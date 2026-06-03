#pragma once

#include <esp_log.h>
#include <esp_system.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "Blynk/BlynkProtocol.h"

#ifndef BLYNK_CONNECT_TIMEOUT_MS
#define BLYNK_CONNECT_TIMEOUT_MS 8000
#endif

#ifndef BLYNK_READ_POLL_MS
#define BLYNK_READ_POLL_MS 50
#endif

#ifndef BLYNK_SEND_POLL_MS
#define BLYNK_SEND_POLL_MS 2000
#endif

class BlynkTransportEsp32
{
public:
    BlynkTransportEsp32()
        : sockfd(-1), domain(NULL), port(0)
    {
    }

    void begin(const char *h, uint16_t p)
    {
        this->domain = h;
        this->port = p;
    }

    bool connect()
    {
        BLYNK_LOG4("Connecting to ", domain, ":", port);

        struct addrinfo hints;
        struct addrinfo *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

        int gai = ::getaddrinfo(domain, port_str, &hints, &res);
        if (gai != 0 || res == NULL)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("DNS resolution failed: ", gai);
#endif
            if (res) freeaddrinfo(res);
            return false;
        }

        sockfd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Failed to create socket: ", errno);
#endif
            freeaddrinfo(res);
            return false;
        }

        int flags = ::fcntl(sockfd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("fcntl failed: ", errno);
#endif
            ::close(sockfd);
            sockfd = -1;
            freeaddrinfo(res);
            return false;
        }

        int cr = ::connect(sockfd, res->ai_addr, res->ai_addrlen);
        if (cr < 0 && errno != EINPROGRESS)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Socket connect failed: ", errno);
#endif
            ::close(sockfd);
            sockfd = -1;
            freeaddrinfo(res);
            return false;
        }
        freeaddrinfo(res);

        if (cr < 0)
        {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sockfd, &wfds);
            struct timeval tv;
            tv.tv_sec = BLYNK_CONNECT_TIMEOUT_MS / 1000;
            tv.tv_usec = (BLYNK_CONNECT_TIMEOUT_MS % 1000) * 1000;

            int sr = ::select(sockfd + 1, NULL, &wfds, NULL, &tv);
            if (sr <= 0)
            {
#ifdef BLYNK_DEBUG
                BLYNK_LOG2("Connect timeout/select: ", sr);
#endif
                ::close(sockfd);
                sockfd = -1;
                return false;
            }

            int so_err = 0;
            socklen_t solen = sizeof(so_err);
            if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &solen) < 0 || so_err != 0)
            {
#ifdef BLYNK_DEBUG
                BLYNK_LOG2("Connect SO_ERROR: ", so_err);
#endif
                ::close(sockfd);
                sockfd = -1;
                return false;
            }
        }

        int one = 1;
        ::setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

#ifdef BLYNK_DEBUG
        BLYNK_LOG1("Successfully connected to server.");
#endif
        return true;
    }

    void disconnect()
    {
        if (sockfd != -1)
        {
            ::close(sockfd);
            sockfd = -1;
#ifdef BLYNK_DEBUG
            BLYNK_LOG1("Socket disconnected.");
#endif
        }
    }

    // Returns: >0 bytes read, 0 if no data within poll window, (size_t)-1 on hard error
    size_t read(void *buf, size_t len)
    {
        if (sockfd < 0) return (size_t)-1;

        uint8_t *p = (uint8_t *)buf;
        size_t got = 0;
        const millis_time_t deadline = BlynkMillis() + BLYNK_READ_POLL_MS;

        while (got < len)
        {
            ssize_t rlen = ::recv(sockfd, p + got, len - got, MSG_DONTWAIT);
            if (rlen > 0)
            {
                got += rlen;
                continue;
            }
            if (rlen == 0)
            {
#ifdef BLYNK_DEBUG
                BLYNK_LOG1("Peer closed connection.");
#endif
                disconnect();
                return (size_t)-1;
            }
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
#ifdef BLYNK_DEBUG
                BLYNK_LOG2("Read error: ", strerror(errno));
#endif
                disconnect();
                return (size_t)-1;
            }

            if (got == 0) return 0; // nothing yet, let caller try again later

            // Partial read: wait briefly for the rest
            millis_time_t now = BlynkMillis();
            if ((int32_t)(deadline - now) <= 0)
            {
                // Couldn't complete; treat as transient — return what we have isn't safe
                // because protocol expects full frame. Drop connection.
#ifdef BLYNK_DEBUG
                BLYNK_LOG2("Read timeout, partial: ", (int)got);
#endif
                disconnect();
                return (size_t)-1;
            }

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sockfd, &rfds);
            struct timeval tv;
            uint32_t remain = deadline - now;
            tv.tv_sec = remain / 1000;
            tv.tv_usec = (remain % 1000) * 1000;
            int sr = ::select(sockfd + 1, &rfds, NULL, NULL, &tv);
            if (sr < 0)
            {
                disconnect();
                return (size_t)-1;
            }
            // loop and retry recv (sr==0 → recheck deadline)
        }
#ifdef BLYNK_DEBUG
        BLYNK_LOG2("Read bytes: ", (int)got);
#endif
        return got;
    }

    size_t write(const void *buf, size_t len)
    {
        if (sockfd < 0) return 0;

        const uint8_t *p = (const uint8_t *)buf;
        size_t sent = 0;
        const millis_time_t deadline = BlynkMillis() + BLYNK_SEND_POLL_MS;

        while (sent < len)
        {
            ssize_t wlen = ::send(sockfd, p + sent, len - sent, MSG_DONTWAIT);
            if (wlen > 0)
            {
                sent += wlen;
                continue;
            }
            if (wlen < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
            {
                millis_time_t now = BlynkMillis();
                if ((int32_t)(deadline - now) <= 0)
                {
#ifdef BLYNK_DEBUG
                    BLYNK_LOG1("Send timeout");
#endif
                    return sent;
                }
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sockfd, &wfds);
                struct timeval tv;
                uint32_t remain = deadline - now;
                tv.tv_sec = remain / 1000;
                tv.tv_usec = (remain % 1000) * 1000;
                int sr = ::select(sockfd + 1, NULL, &wfds, NULL, &tv);
                if (sr < 0)
                {
                    return sent;
                }
                continue;
            }
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Send error: ", errno);
#endif
            return sent;
        }
#ifdef BLYNK_DEBUG
        BLYNK_LOG3("Sent ", (int)sent, "bytes");
#endif
        return sent;
    }

    bool connected()
    {
        return sockfd >= 0;
    }

    int available()
    {
        if (!connected())
            return 0;

        char buf;
        int ret = ::recv(sockfd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (ret > 0)
            return 1;
        if (ret == 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG1("Connection closed by server.");
#endif
            disconnect();
        }
        else if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("recv error: ", strerror(errno));
#endif
            disconnect();
        }
        return 0;
    }

protected:
    int sockfd;
    const char *domain;
    uint16_t port;
};

class BlynkSocket
    : public BlynkProtocol<BlynkTransportEsp32>
{
    typedef BlynkProtocol<BlynkTransportEsp32> Base;

public:
    BlynkSocket(BlynkTransportEsp32 &transp)
        : Base(transp)
    {
    }

    void begin(const char *auth,
               const char *domain = BLYNK_DEFAULT_DOMAIN,
               uint16_t port = BLYNK_DEFAULT_PORT)
    {
        Base::begin(auth);
        this->conn.begin(domain, port);
        const int maxRetries = 500; // 500 x 10ms = ~5 seconds
        int retry = 0;
        while (!this->connected() && retry++ < maxRetries)
        {
            this->run();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
};
