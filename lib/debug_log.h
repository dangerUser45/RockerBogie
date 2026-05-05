#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

class DebugLog : public Print {
public:
    DebugLog();

    void begin(unsigned long baud);
    void beginTelnet(uint16_t port);
    void loop();

    void setWebSocket(AsyncWebSocket* socket);
    void sendRecent(AsyncWebSocketClient* client);

    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;

private:
    static const uint8_t MAX_CLIENTS = 2;
    static const size_t MAX_RECENT_LOG = 6000;

    WiFiServer telnetServer;
    WiFiClient clients[MAX_CLIENTS];
    AsyncWebSocket* logSocket;
    String recentLog;
    bool telnetStarted;

    void acceptTelnetClient();
    void appendRecent(const uint8_t* buffer, size_t size);
};

extern DebugLog Debug;
