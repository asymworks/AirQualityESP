/** 
 * MQTT Air Quality Sensor
 *
 * Copyright (c) 2021 Asymworks, LLC
 * All Rights Reserved
 */

#include <Arduino.h>

#include "TaskScheduler.h"

#include "config.h"
#include "error.h"
#include "mqtt.h"
#include "sensor.h"

// Task Callbacks
void t_discovery();
void t_publish();
void t_read_baseline();
void t_read_data();

//! Module Serial Number
char module_sn[16];

//! Current Sensor Data
SensorData data;

uint16_t sgp30errors = 0;       //< SGP30 Error Count
uint16_t pms5003errors = 0;     //< PMS5003 Error Count

//! Scheduled Task Manager
Scheduler taskManager;

// Scheduled Tasks
Task tDiscovery(TASK_SECOND, TASK_FOREVER, &t_discovery);
Task tReadBaseline(READ_BASELINE_INTERVAL * TASK_MINUTE, TASK_FOREVER, &t_read_baseline);
Task tReadData(READ_SENSOR_INTERVAL * TASK_SECOND, TASK_FOREVER, &t_read_data);
Task tPublish(PUBLISH_INTERVAL * TASK_SECOND, TASK_FOREVER, &t_publish);

//! Send Home Assistant Discovery Messages
void t_discovery() {
    if (!connect_mqtt(module_sn)) {
        haDiscovery(module_sn);

        // Discovery only runs once
        tDiscovery.disable();

        // Enable read data tasks
        tReadBaseline.enable();
        tReadData.enable();
    }
}

//! Publish Data Callback
void t_publish() {
    if (!connect_mqtt(module_sn)) {
        publish_data(& data);
    }
}

//! Sensor Baseline Read Callback
void t_read_baseline() {
    if (!read_baselines() && !connect_mqtt(module_sn)) {
        publish_status("ONLINE");
    }
}

//! Sensor Data Read Callback
void t_read_data() {
    bool errs = false;
    int ret = read_sensors(& data);
    if ((ret & ERROR_SGP30_READ_FAILED) == ERROR_SGP30_READ_FAILED) {
        ++sgp30errors;
        errs = true;
    }
    if ((ret & ERROR_PMS3003_READ_FAILED) == ERROR_PMS3003_READ_FAILED) {
        ++pms5003errors;
        errs = true;
    }

    if (errs && !connect_mqtt(module_sn)) {
#ifdef PUBLISH_ERROR_COUNT
        publish_status("ONLINE");
#endif
    }

    // Ensure we have at least one read before publishing
    if (!tPublish.isEnabled()) {
        tPublish.enable();
    }
}

/** Program Initialization */
void setup() {
    // Initialize Serial Interface
    Serial.begin(DEBUG_BAUD);
    while (!Serial) delay(100);

    delay(1000);
    Serial.println("\n\033[2J");
    Serial.println("Asymworks Air Quality Sensor");

    // Initialize Sensors
    if (setup_sensors(module_sn, sizeof(module_sn))) {
        Serial.println("Sensor Initializion Failed - Please Reset");
        while (1) ;
    }

    // Initialize WiFi and MQTT
    if (setup_mqtt(module_sn)) {
        Serial.println("MQTT Initializion Failed - Please Reset");
        while (1) ;
    }

    // Initialize Task Scheduler
    taskManager.init();
    taskManager.addTask(tDiscovery);
    taskManager.addTask(tPublish);
    taskManager.addTask(tReadBaseline);
    taskManager.addTask(tReadData);

    // The discovery task kicks everything off
    tDiscovery.enableDelayed(1000);
}

/** Main Program Loop */
void loop() {
    if (!connect_mqtt(module_sn)) {
        process_mqtt(MQTT_PROCESS_MS);
    }

    taskManager.execute();
}
