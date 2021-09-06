/** Sensor Processing */

#include <Arduino.h>

#include "Adafruit_BME280.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_SGP30.h"

#include "EEPROM_Rotate.h"

#include "config.h"
#include "error.h"
#include "sensor.h"

// Sensor Objects
Adafruit_BME280 bme;
Adafruit_SGP30 sgp;
Adafruit_PM25AQI aqi;

// Sensor Values
double bmeTemperature = 0;
double bmePressure = 0;
double bmeHumidity = 0;

uint16_t sgpTVOC = 0;
uint16_t sgpeCO2 = 0;

PM25_AQI_Data aqiData;

// Sensor Status
SensorStatus status;

// Global Sensor Status
const SensorStatus * sensor_status = &status;

// EEPROM for Baseline Storage
EEPROM_Rotate eeprom;

// EEPROM Addresses
#define EEPROM_ADDR_ECO2    4
#define EEPROM_ADDR_TVOC    6

// Read Baseline Values from EEPROM
void bl_read() {
    eeprom.size(2);
    eeprom.begin(4096);
    eeprom.get(EEPROM_ADDR_ECO2, status.bl_eCO2);
    eeprom.get(EEPROM_ADDR_TVOC, status.bl_tvoc);
    eeprom.end();

    // If the value is MAX_UINT16 (uninitialized), reset to 0
    if (status.bl_eCO2 == UINT16_MAX) {
        status.bl_eCO2++;
    }
    if (status.bl_tvoc == UINT16_MAX) {
        status.bl_tvoc++;
    }
}

// Write Baseline Values to EEPROM
void bl_write() {
    eeprom.size(2);
    eeprom.begin(4096);
    eeprom.put(EEPROM_ADDR_ECO2, status.bl_eCO2);
    eeprom.put(EEPROM_ADDR_TVOC, status.bl_tvoc);
    eeprom.commit();
    eeprom.end();
}

// Clear Baseline Values in EEPROM
void bl_clear() {
    uint16_t zero = 0;
    eeprom.size(2);
    eeprom.begin(4096);
    eeprom.put(EEPROM_ADDR_ECO2, zero);
    eeprom.put(EEPROM_ADDR_TVOC, zero);
    eeprom.commit();
    eeprom.end();
}

// Setup Sensors
int setup_sensors(char * module_sn, size_t len) {
    memset(&status, 0, sizeof(SensorStatus));

    // Setup BME280
    if (!bme.begin()) {
#ifdef DEBUG
        Serial.println("No BME280 Found - Please Reset");
#endif
        return ERROR_BME280_NOT_FOUND;
    }

#ifdef DEBUG
    Serial.println("Connected to BME280");
#endif

    // Setup SGP30
    if (!sgp.begin()) {
#ifdef DEBUG
        Serial.println("No SGP30 Found - Please Reset");
#endif
        return ERROR_SGP30_NOT_FOUND;
    }

    memset(module_sn, 0, len);
    snprintf(module_sn, len, "%04x%04x%04x", sgp.serialnumber[0], sgp.serialnumber[1], sgp.serialnumber[2]);

#ifdef DEBUG
    Serial.printf("Connected to SGP30 SN %s\n", module_sn);
#endif

    // Setup PM2.5 AQI Sensor
    if (!aqi.begin_I2C())
    {
#ifdef DEBUG
        Serial.println("No AQI-I2C Found - Please Reset");
#endif
        return ERROR_PMS5003_NOT_FOUND;
    }

#ifdef DEBUG
    Serial.println("Connected to PMS5003I");
#endif

    // Read the Baseline Values from EEPROM
    bl_read();
    if (!sgp.setIAQBaseline(status.bl_eCO2, status.bl_tvoc)) {
#ifdef DEBUG
        Serial.println("Could not set SGP30 baseline - Please Reset");
#endif
        return ERROR_SGP30_BASELINE;
    }

#ifdef DEBUG
    Serial.printf("Read Baselines from EEPROM (%d / %d)\n", status.bl_eCO2, status.bl_tvoc);
#endif

    return 0;
}

/* return absolute humidity [mg/m^3] with approximation formula
 * @param temperature [°C]
 * @param humidity [%RH]
 */
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

// Read Sensors
int read_sensors(SensorData * data) {
    int ret = 0;
    PM25_AQI_Data aqiData;

    // Check Return Value
    if (!data) return 1;

    // Process Climate Sensor
    data->temperature = bme.readTemperature();
    data->pressure = bme.readPressure();
    data->humidity = bme.readHumidity();

    // Process Gas Sensor
    sgp.setHumidity(getAbsoluteHumidity(bmeTemperature, bmeHumidity));
    if (sgp.IAQmeasure()) {
        data->tvoc = sgp.TVOC;
        data->eCO2 = sgp.eCO2;
    } else {
        ret |= ERROR_SGP30_READ_FAILED;
        status.sgp30_errors++;
#ifdef DEBUG
        Serial.println("SGP30 Measurement Failed");
#endif
    }

    // Process Particulate Sensor
    if (aqi.read(& aqiData)) {
#ifdef PMS5003_REPORT_ENV
        data->pm10 = aqiData.pm10_env;
        data->pm25 = aqiData.pm25_env;
        data->pm100 = aqiData.pm100_env;
#else
        data->pm10 = aqiData.pm10_standard;
        data->pm25 = aqiData.pm25_standard;
        data->pm100 = aqiData.pm100_standard;
#endif

        data->pc03 = aqiData.particles_03um;
        data->pc05 = aqiData.particles_05um;
        data->pc10 = aqiData.particles_10um;
        data->pc25 = aqiData.particles_25um;
        data->pc50 = aqiData.particles_50um;
        data->pc100 = aqiData.particles_100um;
    } else {
        ret |= ERROR_PMS3003_READ_FAILED;
        status.pms5003_errors++;
#ifdef DEBUG
        Serial.println("PMS5003 Measurement Failed");
#endif
    }

    return ret;
}

int read_baselines() {
    if (!sgp.getIAQBaseline(&status.bl_eCO2, &status.bl_tvoc)) {
        return ERROR_SGP30_READ_FAILED;
    }

    // Write values to EEPROM
    bl_write();

    return 0;
}

int reset_baselines() {
    if (!sgp.setIAQBaseline(0, 0)) {
        return ERROR_SGP30_BASELINE;
    }

    // Reset EEPROM
    bl_clear();

    return 0;
}