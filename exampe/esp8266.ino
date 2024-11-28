#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <vector>

#include "AsyncWebConfig.h"

ESP8266WiFiMulti wifiMulti;
AsyncWebServer server(80);

AsyncWebConfigCfg config = {
    // default config
    { "local ip", "192.168.0.5" },
    { "gateway", "192.168.0.1" },
    { "subnet", "255.255.0.0" },
    { "dns 1", "8.8.8.8" },
    { "dns 2", "8.8.4.4" },
    { "ap 1", "AP1:password" },
    { "ap 2", "AP2:password" },
};

bool config_load()
{
    if (LittleFS.begin()) {
        File file = LittleFS.open("/config.txt", "r");
        if (file) {
            while (file.available()) {
                String name = file.readStringUntil('=');
                String value = file.readStringUntil('\n');
                config.insert_or_assign(name, value);
            }
            file.close();
        }
        LittleFS.end();
        return true;
    }
    return false;
}

bool config_save()
{
    if (LittleFS.begin()) {
        File file = LittleFS.open("/config.txt", "w");
        if (file) {
            for (const auto& element : config)
                file.printf("%s=%s\n", element.first.c_str(), element.second.c_str());
            file.close();
        }
        LittleFS.end();
        return true;
    }
    return false;
}

bool config_delete()
{
    bool done = false;
    if (LittleFS.begin()) {
        if (LittleFS.remove("/config.txt"))
            done = true;
        LittleFS.end();
    }
    return done;
}

void config_cmd_handler(uint8_t* data, size_t len)
{
    String str;
    str.concat((char*)data, len);
    if (str == "save") {
        if (config_save())
            AsyncWebConfig.msg("save: done!");
    } else if (str == "delete") {
        if (config_delete())
            AsyncWebConfig.msg("delete: done!");
    } else if (str == "restart") {
        // AsyncWebConfig.msg("Chip restart!");
        ESP.restart();
        while (true) {
            wdt_reset();
        };
    } else if (str == "help") {
        String str = "available commands:<br>";
        str += "help -> this message<br>";
        str += "save -> save config to filesystem<br>";
        str += "delete -> delete config from filesystem<br>";
        str += "restart -> restart the chip<br>";
        str += "cfg#NewConfig=Value -> create 'NewConfig' with 'Value'<br>";
        str += "others:<br>";
        str += "empty existing config value + submit -> that configuration is deleted<br>";
        str += "!! don't use # in config value !!<br>";
        AsyncWebConfig.msg(str);
    }
}

void init_wifi()
{
    WiFi.mode(WIFI_STA);

    IPAddress local_IP, gateway, subnet, primary_DNS, secondary_DNS;

    local_IP.fromString(config["local ip"]);
    gateway.fromString(config["gateway"]);
    subnet.fromString(config["subnet"]);
    primary_DNS.fromString(config["dns 1"]);
    secondary_DNS.fromString(config["dns 2"]);

    WiFi.config(local_IP, gateway, subnet, primary_DNS, secondary_DNS);

    for (auto element : config) {
        if (element.first.startsWith("ap")) {
            std::vector<String> v;
            str_split(v, element.second, ':');
            if (v[0].length() > 0)
                wifiMulti.addAP(v[0].c_str(), v[1].c_str());
        }
    }

    uint32_t start = millis();
    if (wifiMulti.run(5000) != WL_CONNECTED && millis() - start > 600000) {
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_AP);
        String apName = WiFi.macAddress();
        apName.replace(":", "");
        WiFi.softAP(apName);
    }
}

void setup()
{
    config_load();
    init_wifi();

    AsyncWebConfig.on_cmd(config_cmd_handler);
    AsyncWebConfig.begin(&config, &server, "/config", "admin", "admin");

    server.begin();
}

void loop()
{
    wifiMulti.run();
    delay(1000);
}
