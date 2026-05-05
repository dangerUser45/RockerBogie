#include "debug_log.h"

DebugLog Debug;

DebugLog::DebugLog()
    : telnetServer(23),
      logSocket(nullptr),
      telnetStarted(false) {
}

void DebugLog::begin(unsigned long baud) {
    Serial.begin(baud);
}

void DebugLog::beginTelnet(uint16_t port) {
    (void)port;
    telnetServer.begin();
    telnetServer.setNoDelay(true);
    telnetStarted = true;
    println("[LOG] telnet ready");
}

void DebugLog::setWebSocket(AsyncWebSocket* socket) {
    logSocket = socket;
}

void DebugLog::sendRecent(AsyncWebSocketClient* client) {
    if (client && recentLog.length() > 0) {
        client->text(recentLog);
    }
}

void DebugLog::loop() {
    if (!telnetStarted) return;

    acceptTelnetClient();

    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) continue;
        while (clients[i].available()) {
            clients[i].read();
        }
    }
}

void DebugLog::acceptTelnetClient() {
    if (!telnetServer.hasClient()) return;

    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i] || !clients[i].connected()) {
            if (clients[i]) clients[i].stop();
            clients[i] = telnetServer.available();
            clients[i].println();
            clients[i].println("=== RockerBogie wireless log ===");
            if (recentLog.length() > 0) clients[i].print(recentLog);
            return;
        }
    }

    WiFiClient extraClient = telnetServer.available();
    extraClient.println("Too many log clients");
    extraClient.stop();
}

size_t DebugLog::write(uint8_t c) {
    return write(&c, 1);
}

size_t DebugLog::write(const uint8_t* buffer, size_t size) {
    if (size == 0) return 0;

    Serial.write(buffer, size);

    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i].connected()) {
            clients[i].write(buffer, size);
        }
    }

    appendRecent(buffer, size);

    if (logSocket && logSocket->count() > 0) {
        String chunk;
        chunk.reserve(size + 1);
        for (size_t i = 0; i < size; i++) {
            chunk += (char)buffer[i];
        }
        logSocket->textAll(chunk);
    }

    return size;
}

void DebugLog::appendRecent(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        recentLog += (char)buffer[i];
    }

    if (recentLog.length() > MAX_RECENT_LOG) {
        recentLog.remove(0, recentLog.length() - MAX_RECENT_LOG);
    }
}
