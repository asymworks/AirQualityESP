/** Air Quality Sensor Configuration **/

#ifndef CONFIG_H__
#define CONFIG_H__

//! Baseline Read Interval (minutes)
#define READ_BASELINE_INTERVAL  2

//! Sensor Read Interval (seconds)
#define READ_SENSOR_INTERVAL    1

//! Sensor Publishing Interval (seconds)
#define PUBLISH_INTERVAL        30

//! Publish Error Counts
// #define PUBLISH_ERROR_COUNT

/** Use PMS5003 Environment Values instead of Standard */
#define PMS5003_REPORT_ENV

/** WiFi SSID */
extern const char * wifi_ssid;
extern const char * wifi_passwd;

/** MQTT Connection */
extern const char * mqtt_host;
extern const char * mqtt_user;
extern const char * mqtt_passwd;
extern const char * mqtt_fingerprint;
extern const uint16_t mqtt_port;

/** MQTT Topic Path */
#define MQTT_TOPIC_BASE "sensor/aq"

/** MQTT Use TLS */
#define MQTT_SECURE

/** Number of milliseconds to wait for Subscription Packets */
#define MQTT_PROCESS_MS 100

/** Debug Output to Serial Port */
#define DEBUG
#define DEBUG_BAUD 115200

#endif // CONFIG_H__
