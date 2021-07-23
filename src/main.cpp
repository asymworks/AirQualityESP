#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

#include "Adafruit_BME280.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_SGP30.h"

static const char * wifi_ssid = "kraussnet-ha";
static const char * wifi_passwd = "xN39cY46";

static const char * mqtt_host = "mqtt.ha.kraussnet.com";
static const char * mqtt_user = "sensors";
static const char * mqtt_passwd = "2rsC4Wsg";
static const char * mqtt_fingerprint = "6F 37 49 C6 52 43 FB 45 90 F6 E4 68 7B 43 6D 25 BC 50 F2 8A";

// SGP30 Serial Number (used as MQTT Client Id)
char sgp30_sn[16];

// MQTT Topics
char mqtt_topic_status[40];

char mqtt_topic_echo[40];
char mqtt_topic_reply[40];
char mqtt_topic_cmd[40];

char mqtt_topic_data[40];

// Base MQTT Topic
#define MQTT_TOPIC_BASE "sensor/aq"

// MQTT Reporting Period is MQTT_REPORT_LOOPS * MQTT_PROCESS_MS
#define MQTT_PROCESS_MS 2000    // Number of ms to wait for subscriptions
#define MQTT_REPORT_LOOPS 30    // Number of loops to wait between data publishing

// MQTT JSON Template
#define MQTT_JSON "{" \
  "\"t\":%.1f," \
  "\"p\":%.1f," \
  "\"rh\":%.1f," \
  "\"tvoc\":%d," \
  "\"co2\":%d," \
  "\"h2\":%d," \
  "\"ethanol\":%d," \
  "\"stdPM10\":%d," \
  "\"stdPM25\":%d," \
  "\"stdPM100\":%d," \
  "\"envPM10\":%d," \
  "\"envPM25\":%d," \
  "\"envPM100\":%d," \
  "\"particles03\":%d," \
  "\"particles05\":%d," \
  "\"particles10\":%d," \
  "\"particles25\":%d," \
  "\"particles50\":%d," \
  "\"particles100\":%d" \
"}"

// SSL Client
WiFiClientSecure client;

// MQTT Client
Adafruit_MQTT_Client * mqtt;

// MQTT Feeds
Adafruit_MQTT_Publish * pub_status;
Adafruit_MQTT_Publish * pub_echo;
Adafruit_MQTT_Publish * pub_data;

Adafruit_MQTT_Subscribe * sub_echo;
Adafruit_MQTT_Subscribe * sub_cmd;

// Software Serial Port for PM2.5 Sensor
SoftwareSerial pmSerial(13);

// Sensors
Adafruit_BME280 bme;
Adafruit_SGP30 sgp;
Adafruit_PM25AQI aqi;

// Sensor Values
double bmeTemperature = 0;
double bmePressure = 0;
double bmeHumidity = 0;

uint16_t sgpTVOC = 0;
uint16_t sgpeCO2 = 0;
uint16_t sgpEth = 0;
uint16_t sgpH2 = 0;

PM25_AQI_Data aqiData;
uint16_t aqiErrors = 0;

// SGP30 Baseline Values
uint16_t sgpTVOC_BL = 0;
uint16_t sgpeCO2_BL = 0;

// Loop Counter for MQTT Publish
uint8_t counter = MQTT_REPORT_LOOPS;

// Discovery Messages Sent
bool sentDiscovery = false;

// MQTT Callback for ECHO Topic
void echo_cb(char * data, uint16_t len) {
  pub_echo->publish(data);
  Serial.print("Echo: ");
  Serial.println(data);
}

// MQTT Callback for CMD Topic
void cmd_cb(char * data, uint16_t len) {
  Serial.print("Cmd: ");
  Serial.println(data);
}

// Setup MQTT Client and Topic Paths
void setup_mqtt() {
  mqtt = new Adafruit_MQTT_Client(&client, mqtt_host, 8883, sgp30_sn, mqtt_user, mqtt_passwd);

  // Format Topic Paths
  sprintf(mqtt_topic_status, "%s/%s/%s", MQTT_TOPIC_BASE, sgp30_sn, "status");
  sprintf(mqtt_topic_echo, "%s/%s/%s", MQTT_TOPIC_BASE, sgp30_sn, "echo");
  sprintf(mqtt_topic_reply, "%s/%s/%s", MQTT_TOPIC_BASE, sgp30_sn, "echo/reply");
  sprintf(mqtt_topic_cmd, "%s/%s/%s", MQTT_TOPIC_BASE, sgp30_sn, "cmd");
  sprintf(mqtt_topic_data, "%s/%s/%s", MQTT_TOPIC_BASE, sgp30_sn, "data");

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
}

void setup() {
  uint8_t tmp[26];

  // put your setup code here, to run once:
  // Wait for serial monitor to open
  Serial.begin(115200);
  while (!Serial) delay(100);

  delay(1000);
  Serial.println("\n\033[2J");
  Serial.println("Asymworks Air Quality Sensor");

  // Setup BME280
  if (!bme.begin()) {
    Serial.println("No BME280 Found - Please Reset");
    while (1) ;
  }

  Serial.println("Connected to BME280");

  // Setup SGP30
  if (!sgp.begin()) {
    Serial.println("No SGP30 Found - Please Reset");
    while (1) ;
  }

  memset(sgp30_sn, 0, sizeof(sgp30_sn));
  sprintf(sgp30_sn, "%04x%04x%04x", sgp.serialnumber[0], sgp.serialnumber[1], sgp.serialnumber[2]);

  Serial.printf("Connected to SGP30 SN %s\n", sgp30_sn);

  // Setup PM2.5 AQI Sensor
  Wire.requestFrom(4, 26);
  if (Wire.readBytes(tmp, 26) != 26) {
    Serial.println("No AQI-I2C Found - Please Reset");
    while (1) ;
  }

  // Set MQTT Connection and Topics
  setup_mqtt();

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
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Setup MQTT SSL Fingerprint
  client.setFingerprint(mqtt_fingerprint);
}

void mqtt_connect() {
  int8_t ret;

  if (mqtt->connected()) {
    return;
  }

  Serial.printf("Connecting to MQTT server at %s as %s ", mqtt_host, sgp30_sn);

  uint8_t retries = 3;
  while ((ret = mqtt->connect()) != 0) {
    Serial.print(".");
    mqtt->disconnect();
    delay(5000);
    if (--retries == 0) {
      Serial.println(" Failed - Please Reset");
      Serial.print("Last Return Value: ");
      Serial.println(ret);

      while (1) ;
    }
  }

  Serial.println(" OK");

  // Publish an Online message
  pub_status->publish("online");
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

void readAQI() {
  uint16_t i2cData[13];
  size_t bytesRead;

  memset(i2cData, 0, sizeof(i2cData));

  // Read 26 bytes from the AQI Sensor Arduino
  Wire.requestFrom(4, 26);
  bytesRead = Wire.readBytes(((uint8_t *)i2cData), 26);
  if (bytesRead != 26) {
    Serial.printf("AQI Update Failed (%d bytes read)\n", bytesRead);
  }

  // Move data into aqiData structure
  memset(&aqiData, 0, sizeof(aqiData));
  aqiData.pm10_standard = i2cData[0];
  aqiData.pm25_standard = i2cData[1];
  aqiData.pm100_standard = i2cData[2];
  aqiData.pm10_env = i2cData[3];
  aqiData.pm25_env = i2cData[4];
  aqiData.pm100_env = i2cData[5];
  aqiData.particles_03um = i2cData[6];
  aqiData.particles_05um = i2cData[7];
  aqiData.particles_10um = i2cData[8];
  aqiData.particles_25um = i2cData[9];
  aqiData.particles_50um = i2cData[10];
  aqiData.particles_100um = i2cData[11];

  // Store Error Count
  aqiErrors = i2cData[12];
}

void haRegisterSensor(
  const char * name,
  const char * units,
  const char * deviceClass,
  const char * valueKey
) {
  char cfgSensorName[40];
  char cfgTopic[80];
  char cfgData[MAXBUFFERSIZE-20];
  char cfgMessage[MAXBUFFERSIZE];

  snprintf(cfgSensorName, 39, "aq_%s_%s", sgp30_sn, name);
  snprintf(cfgTopic, 79, "homeassistant/sensor/%s/config", cfgSensorName);
  snprintf(cfgData, MAXBUFFERSIZE-21, "\"name\":\"%s\",\"uniq_id\":\"%s\",\"unit_of_meas\":\"%s\",\"stat_t\":\"%s/%s/data\",\"val_tpl\":\"{{ value_json.%s }}\"",
    cfgSensorName,
    cfgSensorName,
    units,
    MQTT_TOPIC_BASE,
    sgp30_sn,
    valueKey
  );

  if (deviceClass) {
    snprintf(cfgMessage, MAXBUFFERSIZE-1, "{\"dev_cla\":\"%s\",%s}", deviceClass, cfgData);
  } else {
    snprintf(cfgMessage, MAXBUFFERSIZE-1, "{%s}", cfgData);
  }

  Serial.printf("%s %s\n", cfgTopic, cfgMessage);
  mqtt->publish(cfgTopic, cfgMessage);
}

void haDiscovery() {
  // Send Home Assistant Discovery Messages to MQTT
  haRegisterSensor("temperature", "°C", "temperature", "t");
  haRegisterSensor("pressure", "Pa", "pressure", "p");
  haRegisterSensor("humidity", "%", "humidity", "rh");

  haRegisterSensor("tvoc", "ppb", 0, "tvoc");
  haRegisterSensor("eco2", "ppm", "carbon_dioxide", "co2");

  haRegisterSensor("pm10", "µg/m³", 0, "envPM10");
  haRegisterSensor("pm25", "µg/m³", 0, "envPM25");
  haRegisterSensor("pm100", "µg/m³", 0, "envPM100");
}

void loop() {
  uint16_t aqiLastErrors = aqiErrors;

  // put your main code here, to run repeatedly:
  mqtt_connect();
  if (!mqtt->ping()) {
    mqtt->disconnect();
  }

  // Send Discovery Message
  if (!sentDiscovery) {
    haDiscovery();
    sentDiscovery = true;
  }

  // Process Subscriptions
  mqtt->processPackets(MQTT_PROCESS_MS);

  // Process Sensors
  bmeTemperature = bme.readTemperature();
  bmePressure = bme.readPressure();
  bmeHumidity = bme.readHumidity();

  sgp.setHumidity(getAbsoluteHumidity(bmeTemperature, bmeHumidity));
  if (sgp.IAQmeasure()) {
    sgpTVOC = sgp.TVOC;
    sgpeCO2 = sgp.eCO2;
  } else {
    Serial.println("SGP30 Measurement Failed");
  }

  if (sgp.IAQmeasureRaw()) {
    sgpEth = sgp.rawEthanol;
    sgpH2 = sgp.rawH2;
  } else {
    Serial.println("SGP30 Raw Measurement Failed");
  }

  readAQI();
  if (aqiErrors > aqiLastErrors) {
    char errMsg[80];
    snprintf(errMsg, 80, "AQI Error Count: %d", aqiErrors);
    Serial.println(errMsg);
    pub_status->publish(errMsg);
  }

  // Publish to MQTT (replace with scheduler)
  if (!(--counter)) {
    char json[1024];

    // Format JSON Message for MQTT and Publish
    snprintf(
      json, 1023, MQTT_JSON,
      bmeTemperature,
      bmePressure,
      bmeHumidity,
      sgpTVOC,
      sgpeCO2,
      sgpH2,
      sgpEth,
      aqiData.pm10_standard,
      aqiData.pm25_standard,
      aqiData.pm100_standard,
      aqiData.pm10_env,
      aqiData.pm25_env,
      aqiData.pm100_env,
      aqiData.particles_03um,
      aqiData.particles_05um,
      aqiData.particles_10um,
      aqiData.particles_25um,
      aqiData.particles_50um,
      aqiData.particles_100um
    );
    pub_data->publish(json);

    // Print raw sensor values to serial
    Serial.printf("BME: %.2f C / %.2f Pa / %.2f RH%%\n",
      bmeTemperature,
      bmePressure,
      bmeHumidity
    );
    Serial.printf("SGP30: %d TVOC / %d eCO2 / %d Ethanol / %d H2\n",
      sgpTVOC,
      sgpeCO2,
      sgpEth,
      sgpH2
    );
    Serial.printf("AQI (Std): %d PM1.0 / %d PM2.5 / %d PM10\n",
      aqiData.pm10_standard,
      aqiData.pm25_standard,
      aqiData.pm100_standard
    );
    Serial.printf("AQI (Env): %d PM1.0 / %d PM2.5 / %d PM10\n",
      aqiData.pm10_env,
      aqiData.pm25_env,
      aqiData.pm100_env
    );
    Serial.printf("Particles: %d 0.3 / %d 0.5 / %d 1.0 / %d 2.5 / %d 5.0 / %d 10.0\n",
      aqiData.particles_03um,
      aqiData.particles_05um,
      aqiData.particles_10um,
      aqiData.particles_25um,
      aqiData.particles_50um,
      aqiData.particles_100um
    );

    // Restart loop counter
    counter = MQTT_REPORT_LOOPS;
  }
}