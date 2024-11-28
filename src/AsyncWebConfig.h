#ifndef AsyncWebConfig_h
#define AsyncWebConfig_h

#include <Arduino.h>

#include "config.min.html.gz.h"
#include "str_split.h"
#include <map>
#include <vector>

#if defined(ESP8266)
#include "ESP8266WiFi.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#elif defined(ESP32)
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "WiFi.h"
#endif

#if defined(ARDUINO_ARCH_ESP8266)
#if defined(DEBUG_ESP_PORT) && defined(ASYNC_WEBCONFIG_DEBUG)
#define log_i(...)                      \
    DEBUG_ESP_PORT.printf(__VA_ARGS__); \
    DEBUG_ESP_PORT.print("\n")
#define log_e(...)                      \
    DEBUG_ESP_PORT.printf(__VA_ARGS__); \
    DEBUG_ESP_PORT.print("\n")
#define log_w(...)                      \
    DEBUG_ESP_PORT.printf(__VA_ARGS__); \
    DEBUG_ESP_PORT.print("\n")
#else
#define log_i(...)
#define log_e(...)
#define log_w(...)
#endif
#endif

typedef std::function<void(uint8_t* data, size_t len)> AsyncWebConfigCmdHandler;
typedef std::map<String, String> AsyncWebConfigCfg;

class AsyncWebConfigClass {

public:
    void begin(AsyncWebConfigCfg* config, AsyncWebServer* server, const char* url = "/config", const char* username = "", const char* password = "");
    void on_cmd(AsyncWebConfigCmdHandler cb) { _cmd_cb = cb; }
    void loop() { _ws->cleanupClients(); };
    void msg(const char* message);
    void msg(const String& message);

private:
    void handle_ws_message(void* arg, uint8_t* data, size_t len, AsyncWebSocketClient* client);
    void send_config();

    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    AsyncWebSocketClient* _ws_client = NULL;
    AsyncWebConfigCmdHandler _cmd_cb = NULL;

    String _username = "";
    String _password = "";
    bool _authRequired = false;
    AsyncWebConfigCfg* _config;
};

void AsyncWebConfigClass::begin(AsyncWebConfigCfg* config, AsyncWebServer* server, const char* url, const char* username, const char* password)
{
    _config = config;
    _server = server;

    if (strlen(username) > 0) {
        _authRequired = true;
        _username = username;
        _password = password;
    } else {
        _authRequired = false;
        _username = "";
        _password = "";
    }

    _server->on(url, HTTP_GET, [&](AsyncWebServerRequest* request) {
        if (_authRequired)
            if (!request->authenticate(_username.c_str(), _password.c_str()))
                return request->requestAuthentication();
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", config_min_html_gz, config_min_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    String wsUrl = url;
    wsUrl += "ws";

    _ws = new AsyncWebSocket(wsUrl);

    if (_authRequired)
        _ws->setAuthentication(_username.c_str(), _password.c_str());

    _ws->onEvent([&](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) -> void {
        if (type == WS_EVT_CONNECT) {
            _ws_client = client;
            log_i("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
            send_config();
        } else if (type == WS_EVT_DISCONNECT) {
            _ws_client = NULL;
            log_i("WebSocket client #%u disconnected", client->id());
        } else if (type == WS_EVT_DATA) {
            log_i("WebSocket data received from client: #%u", client->id());
            handle_ws_message(arg, data, len, client);
        } else if (type == WS_EVT_PONG) {
            log_i("WebSocket pong");
        } else if (type == WS_EVT_ERROR) {
            log_i("WebSocket error");
        }
        _ws->cleanupClients();
    });

    _server->addHandler(_ws);
}

void AsyncWebConfigClass::handle_ws_message(void* arg, uint8_t* data, size_t len, AsyncWebSocketClient* client)
{
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        bool done = false;
        std::vector<String> m;
        str_split(m, (char*)data, len, '#');
        if (m.size() > 0) {
            if (m[0] == "cfg") {
                if (m.size() == 2) {
                    std::vector<String> c;
                    str_split(c, m[1], '=');
                    if (c.size() == 2) {
                        if (c[1].length() > 0)
                            _config->insert_or_assign(c[0], c[1]);
                        else {
                            _config->erase(c[0]);
                            msg("'" + c[0] + "' delete from config");
                        }
                        done = true;
                    }
                }
            }
        }
        if (done)
            msg("Done");
        else {
            if (_cmd_cb != NULL) {
                _cmd_cb(data, len);
            }
        }
    }
}

void AsyncWebConfigClass::send_config()
{
    String str = "cfg";
    for (const auto& element : *_config) {
        str += "#";
        str += element.first.c_str();
        str += "=";
        str += element.second.c_str();
    }
    if (str.length() > 3)
        _ws_client->text(str);
    //_ws_client->printf("msg#heap: %d", ESP.getFreeHeap());
}

void AsyncWebConfigClass::msg(const char* message)
{
    if (_ws_client != NULL && _ws_client->status() == WS_CONNECTED)
        _ws_client->printf("msg#%s", message);
}

void AsyncWebConfigClass::msg(const String& message)
{
    msg(message.c_str());
}


AsyncWebConfigClass AsyncWebConfig;

#endif
