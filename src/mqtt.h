/** MQTT Routines */

#ifndef MQTT_H__
#define MQTT_H__

#include "sensor.h"

/**
 * Setup the WiFi and MQTT Connection
 * @param [in] module_sn module serial number string
 * @return zero if initialization succeeded, or non-zero if an error occurred
 */
int setup_mqtt(const char *);

/**
 * Connect to the MQTT Server
 * @param [in] module_sn module serial number string
 * @return zero if connection succeeded, or non-zero if an error occurred
 */
int connect_mqtt(const char *);

/**
 * Process MQTT Subscription Packets
 * @param [in] timeout timeout in milliseconds
 */
void process_mqtt(int16_t);

/**
 * Send Home Assistant Discovery MQTT messages
 * @param [in] module_sn module serial number string
 */
void haDiscovery(const char *);

/**
 * Report sensor data.
 * @param [in] data Sensor data
 */
void publish_data(const SensorData *);

/**
 * Report status data.
 * @param [in] status Status message
 */
void publish_status(const char *);

#endif // MQTT_H__
