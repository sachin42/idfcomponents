#pragma once

#include <esp_log.h>
#include <esp_system.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <unistd.h>
#include <string.h>
#include "Blynk/BlynkProtocol.h"

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

        struct sockaddr_in server_addr;
        struct hostent *server = gethostbyname(domain);
        if (server == NULL)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("DNS resolution failed for ", domain);
#endif
            return false;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, server->h_addr_list[0], ip_str, sizeof(ip_str));
        // ESP_LOGI("BLYNK", "Resolved IP: %s", ip_str);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr = *((struct in_addr *)server->h_addr);
        memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Failed to create socket: ", errno);
#endif
            return false;
        }

        int res = ::connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (res < 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Socket connect failed: ", errno);
#endif
            ::close(sockfd);
            sockfd = -1;
            return false;
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0; // 1ms timeout for reading
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

        int one = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef BLYNK_DEBUG
        BLYNK_LOG1("Successfully connected to server.");
#endif
        return true;
    }

    void disconnect()
    {
        if (sockfd != -1)
        {
            close(sockfd);
            sockfd = -1;
#ifdef BLYNK_DEBUG
            BLYNK_LOG1("Socket disconnected.");
#endif
        }
    }

    size_t read(void *buf, size_t len)
    {
        ssize_t rlen = ::recv(sockfd, buf, len, 0);
        if (rlen < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return 0;
            }
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Read error: ", strerror(errno));
#endif
            disconnect();
            return -1;
        }
#ifdef BLYNK_DEBUG
        BLYNK_LOG2("Read bytes: ", rlen);
#endif
        return rlen;
    }

    size_t write(const void *buf, size_t len)
    {
        ssize_t wlen = ::send(sockfd, buf, len, 0);
        if (wlen < 0)
        {
#ifdef BLYNK_DEBUG
            BLYNK_LOG2("Send error: ", errno);
#endif
            return 0;
        }
#ifdef BLYNK_DEBUG
        BLYNK_LOG3("Sent ", (int)wlen, "bytes");
#endif
        return wlen;
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
        const int maxRetries = 500; // 500 x 10ms = ~5 seconds timeout
        int retry = 0;
        while (!this->connected() && retry++ < maxRetries)
        {
            this->run();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
};
