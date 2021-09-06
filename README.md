# ESP8266 Air Quality Sensor

Arduino code to implement a MQTT-enabled air quality sensor using an ESP8266 (NodeMCU) microcontroller, BME280 climate sensor, SGP30 gas sensor, and PMA5003I particulate matter sensor. It is designed for use with Home Assistant and supports auto-discovery of all the measurands.

## Hardware Configuration

Fritzing diagram is coming soon... basically a [BME280](https://www.adafruit.com/product/2652), [SGP30](https://www.adafruit.com/product/3709), and [PMSA5003I](https://www.adafruit.com/product/4632) connected to the hardware SPI bus on a [NodeMCU v1.1](https://www.nodemcu.com/index_en.html).

## Software Configuration

The `config.cpp-sample` file should be copied and renamed to `config.cpp`, and the following settings filled in (these sensitive values should not be stored in source control):

```cpp
const char * wifi_ssid = "ssid-iot";
const char * wifi_passwd = "ssid-psk";
```

Set these to match the SSID and PSK you wish to use to connect this sensor.

```cpp
const char * mqtt_host = "mqtt.local";
const char * mqtt_user = "sensors";
const char * mqtt_passwd = "sensors";
const char * mqtt_fingerprint = "01 23 45 67 89 AB CD EF 01 23 45 67 89 AB CD EF 01 23 45 67";
const uint16_t mqtt_port = MQTT_DEFAULT_PORT
```

Set these to match the ACL of your MQTT broker. Anonymous connection to the MQTT broker is not currently supported. TLS connections are supported by setting the `mqtt_fingerprint` variable to the SHA1 fingerprint of the X509 certificate served by the MQTT broker. [See here](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html#setfingerprint-const-uint8-t-fp-20-setfingerprint-const-char-fpstr) for more details. TLS connections can be disabled by commenting out the `MQTT_SECURE` definition in `config.h`.

Additional settings are in `config.h`:

```cpp
#define READ_BASELINE_INTERVAL  2
#define READ_SENSOR_INTERVAL    1
#define PUBLISH_INTERVAL        30
```

These define the intervals of the main sensor tasks. `READ_BASELINE_INTERVAL` sets the interval *in minutes* between reading the baseline values of the SGP30 sensor and writing them to EEPROM. The default is to store them every hour as recommended by the datasheet. The baseline values are persisted across resets. `READ_SENSOR_INTERVAL` is the polling interval for the sensors themselves, and `PUBLISH_INTERVAL` is how often new values are sent to MQTT. These two settings are in *seconds* and default to polling the sensors once per second and publishing data every 30 seconds.

```cpp
// #define PUBLISH_ERROR_COUNT
```

Uncomment this line to send PMS5003I communication error counts to MQTT on every sensor polling interval. By default these are reported along with the baseline settings every hour. Note this is in the code because I did not have a PMS5003I sensor originally and use a second Arduino to perform an I2C to UART bridge to emulate the I2C interface of the PMS5003I. Hopefully the actual I2C board does not have nearly as many communication errors as mine.

```cpp
#define PMS5003_REPORT_ENV
```

Comment this line to report the "standard atmosphere" values from the PMS5003 instead of the compensated values.

```cpp
#define MQTT_TOPIC_BASE "sensor/aq"
```

This defines the MQTT base topic.  MQTT topics have the form `${MQTT_TOPIC_BASE}/${SGP30_SN}/${ENDPOINT}` where `SGP30_SN` is the serial number of the SGP30 sensor (used as the module serial number) and `${ENDPOINT}` is the topic endpoint. See the [MQTT Endpoints](#mqtt-endpoints) section for more details.

```cpp
#define MQTT_SECURE
```

Comment this line to use an insecure connection to the MQTT server.

```cpp
#define MQTT_PROCESS_MS 100
```

This sets the timeout for the `Arduino_MQTT_Client::processPackets()` call to process topic subscriptions. This gets called in the main loop so should be set relatively short (and definintely shorter than the minimum task interval for sensor polling or reporting).

```cpp
#define DEBUG
#define DEBUG_BAUD 115200
```

Comment out the first line to disable debug logging to the serial port, or change `DEBUG_BAUD` to the preferred baud rate of your serial terminal (change `platformio.ini` as well if you are using the integrated terminal emulator).

## MQTT Endpoints

There are five MQTT endpoints defined for this sensor:
- `${MQTT_TOPIC_BASE}/${SGP30_SN}/data` : JSON objects containing sensor data are published to this endpoint by the
    sensor, every `PUBLISH_INTERVAL` seconds. The JSON structure is as follows:

    ```json
    {
        "t": 29.3,
        "p": 100794.4,
        "rh": 37.1,
        "tvoc": 8,
        "co2": 400,
        "pm10": 12,
        "pm25": 19,
        "pm100": 22,
        "particles03": 2163,
        "particles05": 634,
        "particles10": 134,
        "particles25": 11,
        "particles50": 5,
        "particles100": 1
    }
    ```

    The reported values include temperature in degrees celsius, pressure in mbar, relative humidity in percent, total VOC 
    concentration in ppb, effective CO2 concentration in ppb, particulate matter concentration (1.0µm, 2.5µm, and 10µm 
    sizes) in µg/m³, and actual particle counts for the 0.3µm, 0.5µm, 1.0µm, 2.5µm, 5.0µm, and 10µm size buckets.

- `${MQTT_TOPIC_BASE}/${SGP30_SN}/status` : JSON objects containing sensor status are published to this endpoint by the
    sensor, every `READ_BASELINE_INTERVAL` minutes. The JSON structure is as follows:
    
    ```json
    {
        "status": "ONLINE",
        "sgp30_errors": 0,
        "pms5003_errors": 69,
        "bl_tvoc": 37545,
        "bl_eco2": 37744
    }
    ```

    The `status` field is always set to `ONLINE` currently. The number of errors reported by the SGP30 and PMS5003 
    interfaces are reported in their respective fields, and the current SGP30 baseline values are reported as well.

- `${MQTT_TOPIC_BASE}/${SGP30_SN}/echo` : This is used as a sensor health check. Data published to this endpoint is
    sent back on the `${MQTT_TOPIC_BASE}/${SGP30_SN}/echo/reply` endpoint.

- `${MQTT_TOPIC_BASE}/${SGP30_SN}/echo/reply` : Response endpoint for echo data.

- `${MQTT_TOPIC_BASE}/${SGP30_SN}/cmd` : Sensor command interface. Currently only a `resetBaselines` command is
    implemented, which resets the SGP30 baseline values to 0. Send `resetBaselines` as a string to this endpoint
    to reset the baselines.

## License

MIT License

## Author

This projects was created in 2021 by Jonathan Krauss (@asymworks).
