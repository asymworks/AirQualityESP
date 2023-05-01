/** MQTT Support Routines */

#include <ESP8266WiFi.h>
#include <strings.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "config.h"
#include "error.h"
#include "mqtt.h"

// MQTT JSON Template
#define MQTT_JSON "{" \
    "\"t\":%.1f," \
    "\"p\":%.1f," \
    "\"rh\":%.1f," \
    "\"tvoc\":%d," \
    "\"co2\":%d," \
    "\"pm10\":%d," \
    "\"pm25\":%d," \
    "\"pm100\":%d," \
    "\"particles03\":%d," \
    "\"particles05\":%d," \
    "\"particles10\":%d," \
    "\"particles25\":%d," \
    "\"particles50\":%d," \
    "\"particles100\":%d" \
"}"

// MQTT Status JSON Template
#define MQTT_STATUS_JSON "{" \
    "\"status\":\"%s\"," \
    "\"sgp30_errors\":%d," \
    "\"pms5003_errors\":%d," \
    "\"bl_tvoc\":%d," \
    "\"bl_eco2\":%d" \
"}"

// SSL Client
#ifdef MQTT_SECURE
WiFiClientSecure client;
#else
WiFiClient client;
#endif

// MQTT Client
Adafruit_MQTT_Client * mqtt;

// MQTT Topic Strings
char mqtt_topic_status[40];

char mqtt_topic_echo[40];
char mqtt_topic_reply[40];
char mqtt_topic_cmd[40];

char mqtt_topic_data[40];

// MQTT Feeds
Adafruit_MQTT_Publish * pub_status;
Adafruit_MQTT_Publish * pub_echo;
Adafruit_MQTT_Publish * pub_data;

Adafruit_MQTT_Subscribe * sub_echo;
Adafruit_MQTT_Subscribe * sub_cmd;

// MQTT Callback for ECHO Topic
void echo_cb(char * data, uint16_t len) {
    pub_echo->publish(data);
    Serial.print("Echo: ");
    Serial.println(data);
}

// MQTT Callback for CMD Topic
void cmd_cb(char * data, uint16_t len) {
    if (!strcasecmp(data, "resetBaseline")) {
        Serial.print("Resetting Baseline Values... ");
        if (!reset_baselines()) {
            Serial.println("OK");
        } else {
            Serial.println("Failed");
        }
    } else {
        Serial.printf("Ignoring unknown command: \"%s\"\n", data);
    }
}

// Setup MQTT
int setup_mqtt(const char * module_sn) {
    mqtt = new Adafruit_MQTT_Client(&client, mqtt_host, mqtt_port, module_sn, mqtt_user, mqtt_passwd);

    // Format Topic Paths
    sprintf(mqtt_topic_status, "%s/%s/%s", MQTT_TOPIC_BASE, module_sn, "status");
    sprintf(mqtt_topic_echo, "%s/%s/%s", MQTT_TOPIC_BASE, module_sn, "echo");
    sprintf(mqtt_topic_reply, "%s/%s/%s", MQTT_TOPIC_BASE, module_sn, "echo/reply");
    sprintf(mqtt_topic_cmd, "%s/%s/%s", MQTT_TOPIC_BASE, module_sn, "cmd");
    sprintf(mqtt_topic_data, "%s/%s/%s", MQTT_TOPIC_BASE, module_sn, "data");

#ifdef DEBUG
    Serial.printf("MQTT Status: %s\n", mqtt_topic_status);
    Serial.printf("MQTT Echo: %s\n", mqtt_topic_echo);
    Serial.printf("MQTT Reply: %s\n", mqtt_topic_reply);
    Serial.printf("MQTT Command: %s\n", mqtt_topic_cmd);
    Serial.printf("MQTT Data: %s\n", mqtt_topic_data);
#endif

    // Create Pub/Sub Objects
    pub_status = new Adafruit_MQTT_Publish(mqtt, mqtt_topic_status);
    pub_echo = new Adafruit_MQTT_Publish(mqtt, mqtt_topic_reply);
    pub_data = new Adafruit_MQTT_Publish(mqtt, mqtt_topic_data);

    sub_echo = new Adafruit_MQTT_Subscribe(mqtt, mqtt_topic_echo);
    sub_cmd = new Adafruit_MQTT_Subscribe(mqtt, mqtt_topic_cmd);

    // Setup Subscriber Callbacks
    sub_echo->setCallback(echo_cb);
    sub_cmd->setCallback(cmd_cb);

    // Subscribe to Topics
    mqtt->subscribe(sub_echo);
    mqtt->subscribe(sub_cmd);

    // Startup WiFi
    WiFi.begin(wifi_ssid, wifi_passwd);
    Serial.print("Connecting to ");
    Serial.print(wifi_ssid);
    Serial.print("");

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println("WiFi Connected");
#ifdef DEBUG
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
#endif

#ifdef MQTT_SECURE
    // Setup MQTT SSL Fingerprint
    client.setFingerprint(mqtt_fingerprint);
#endif

    return 0;
}

// Connect to the MQTT Server
int connect_mqtt(const char * module_sn) {
    int8_t ret;

    // Ping the MQTT server
    if (!mqtt->ping()) {
        mqtt->disconnect();
    }

    // If connected, skip re-connect
    if (mqtt->connected()) {
        return 0;
    }

    // Reconnect to the server
    Serial.printf("Connecting to MQTT server at %s:%d as %s ", mqtt_host, mqtt_port, module_sn);

    uint8_t retries = 3;
    while ((ret = mqtt->connect()) != 0) {
        Serial.print(".");
        mqtt->disconnect();
        delay(5000);

        if (--retries == 0) {
            Serial.println(" Failed - Please Reset");
            Serial.print("Last Return Value: ");
            Serial.println(ret);
            
            return ERROR_MQTT_CONNECT_FAILED;
        }
    }

    Serial.println(" OK");
    return 0;
}

// Process MQTT Subscriptions
void process_mqtt(int16_t timeout) {
    if (mqtt->connected()) {
        mqtt->processPackets(timeout);
    }
}

// Send Home Assistant Discovery for one Sensor
void haRegisterSensor(
    const char * module_sn,
    const char * topic,
    const char * name,
    const char * units,
    const char * deviceClass,
    const char * valueKey,
    bool retain = true
) {
    char cfgSensorName[40];
    char cfgTopic[80];
    char cfgDev[120];
    char cfgData[MAXBUFFERSIZE-150];
    char cfgMessage[MAXBUFFERSIZE];

    snprintf(cfgSensorName, 39, "aq_%s_%s", module_sn, name);
    snprintf(cfgTopic, 79, "homeassistant/sensor/%s/config", cfgSensorName);
    snprintf(cfgDev, 119, "\"ids\":[\"aq_%s\"],\"mf\":\"Asymworks, LLC\",\"mdl\":\"AirQualityESP\",\"name\":\"AirQuality ESP %s\"", module_sn, module_sn);
    snprintf(cfgData, MAXBUFFERSIZE-21, "\"name\":\"%s\",\"uniq_id\":\"%s\",\"unit_of_meas\":\"%s\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.%s }}\"",
        cfgSensorName,
        cfgSensorName,
        units,
        topic,
        valueKey
    );

    if (deviceClass) {
        snprintf(cfgMessage, MAXBUFFERSIZE-1, "{\"dev_cla\":\"%s\",%s,\"dev\":{%s}}", deviceClass, cfgData, cfgDev);
    } else {
        snprintf(cfgMessage, MAXBUFFERSIZE-1, "{%s,\"dev\":{%s}}", cfgData, cfgDev);
    }

#ifdef DEBUG
    Serial.printf("%s %s\n", cfgTopic, cfgMessage);
#endif

    mqtt->publish(cfgTopic, cfgMessage, 0, retain);
}

// Send Home Assistant Discovery for all sensors
void haDiscovery(const char * module_sn, bool retain) {
    haRegisterSensor(module_sn, mqtt_topic_data, "temperature", "°C", "temperature", "t", retain);
    haRegisterSensor(module_sn, mqtt_topic_data, "pressure", "Pa", "pressure", "p", retain);
    haRegisterSensor(module_sn, mqtt_topic_data, "humidity", "%", "humidity", "rh", retain);

    haRegisterSensor(module_sn, mqtt_topic_data, "tvoc", "ppb", 0, "tvoc", retain);
    haRegisterSensor(module_sn, mqtt_topic_data, "eco2", "ppm", "carbon_dioxide", "co2", retain);

    haRegisterSensor(module_sn, mqtt_topic_data, "pm10", "µg/m³", 0, "pm10", retain);
    haRegisterSensor(module_sn, mqtt_topic_data, "pm25", "µg/m³", 0, "pm25", retain);
    haRegisterSensor(module_sn, mqtt_topic_data, "pm100", "µg/m³", 0, "pm100", retain);

    haRegisterSensor(module_sn, mqtt_topic_status, "sgp_errors", " ", 0, "sgp30_errors");
    haRegisterSensor(module_sn, mqtt_topic_status, "aqi_errors", " ", 0, "pms5003_errors");
    haRegisterSensor(module_sn, mqtt_topic_status, "baseline_eco2", " ", 0, "bl_eco2");
    haRegisterSensor(module_sn, mqtt_topic_status, "baseline_tvoc", " ", 0, "bl_tvoc");
}

// Send JSON Data to MQTT
void publish_data(const SensorData * data) {
    char json[1024];
    snprintf(
        json, 1023, MQTT_JSON,
        data->temperature,
        data->pressure,
        data->humidity,
        data->tvoc,
        data->eCO2,
        data->pm10,
        data->pm25,
        data->pm100,
        data->pc03,
        data->pc05,
        data->pc10,
        data->pc25,
        data->pc50,
        data->pc100
    );

    pub_data->publish(json);
}

// Send JSON Sensor Status to MQTT
void publish_status(const char * status) {
    char json[1024];
    snprintf(
        json, 1023, MQTT_STATUS_JSON,
        status,
        sensor_status->sgp30_errors,
        sensor_status->pms5003_errors,
        sensor_status->bl_tvoc,
        sensor_status->bl_eCO2
    );

    pub_status->publish(json);
}
