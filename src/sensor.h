/** Air Quality Sensor - Sensor Interface */

#ifndef SENSOR_H__
#define SENSOR_H__

#include <stddef.h>

//! Sensor Data Structure
typedef struct {
    // BME280
    double temperature;
    double pressure;
    double humidity;

    // SGP30
    uint16_t tvoc;
    uint16_t eCO2;

    // PMS5003I PM
    uint16_t pm10;
    uint16_t pm25;
    uint16_t pm100;

    // PMS5003I Particle Count
    uint16_t pc03;
    uint16_t pc05;
    uint16_t pc10;
    uint16_t pc25;
    uint16_t pc50;
    uint16_t pc100;

} SensorData;

//! Sensor Status Structure
typedef struct {
    // SGP30 Baseline
    uint16_t bl_tvoc;
    uint16_t bl_eCO2;

    // Error Counts
    uint32_t sgp30_errors;
    uint32_t pms5003_errors;

} SensorStatus;

//! Global Sensor Status
extern const SensorStatus * sensor_status;

/**
 * Initialize the Sensors
 * @param [out] sensor_sn sensor serial number buffer
 * @param [in] length length of the sensor serial number buffer
 * @return zero if initialization succeeded, or non-zero if an error occurred
 *
 * Initializes the air quality sensor suite and sets the module serial number,
 * which is read from the SGP30 sensor. The serial number is a formatted 
 * 12-character hex string, so the character buffer should be at least 13 bytes
 * in length.
 */
int setup_sensors(char *, size_t);

/**
 * Read Sensor Data
 * @param [out] data current sensor data
 * @return zero if the read succeeded, or non-zero if an error occurred
 * 
 * Reads and copies the current sensor data into the output data structure. The
 * return value on failure is a bitmask of ERROR_SGP30_READ_FAILED and
 * ERROR_PMS3003_READ_FAILED (or 1, indicating an invalid argument).
 */
int read_sensors(SensorData *);

/** Read Current Sensor Baselines */
int read_baselines();

/** Reset Sensor Baseline */
int reset_baselines();

#endif // SENSOR_H__
