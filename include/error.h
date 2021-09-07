/** Air Quality Sensor Error Codes */

#ifndef ERROR_H__
#define ERROR_H__

#define ERROR_BME280_NOT_FOUND      10
#define ERROR_SGP30_NOT_FOUND       11
#define ERROR_SGP30_BASELINE        12
#define ERROR_PMS5003_NOT_FOUND     13

#define ERROR_MQTT_CONNECT_FAILED   20

#define ERROR_SGP30_READ_FAILED     (1 << 2)
#define ERROR_PMS3003_READ_FAILED   (1 << 3)

#endif // ERROR_H__
