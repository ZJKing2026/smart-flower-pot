#include <Wire.h>
#include <Adafruit_SHT31.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t SHT30_ADDRESS = 0x44;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SAMPLE_INTERVAL_MS = 2000;
constexpr float MIN_TEMPERATURE_C = -40.0F;
constexpr float MAX_TEMPERATURE_C = 125.0F;
constexpr float MIN_HUMIDITY_PERCENT = 0.0F;
constexpr float MAX_HUMIDITY_PERCENT = 100.0F;

Adafruit_SHT31 sht30(&Wire);
bool sensorReady = false;

/**
 * 读取并校验一组温湿度数据，然后输出到串口。
 *
 * @param sampleCount 当前采样编号。
 * @return 数据有效时返回 true，否则返回 false。
 */
bool readAndPrintMeasurement(unsigned long sampleCount) {
  const float temperatureC = sht30.readTemperature();
  const float humidityPercent = sht30.readHumidity();

  Serial.printf("Sample #%lu\n", sampleCount);
  if (isnan(temperatureC) || isnan(humidityPercent)) {
    Serial.println("SHT30 read failed: NaN received.\n");
    return false;
  }

  if (temperatureC < MIN_TEMPERATURE_C ||
      temperatureC > MAX_TEMPERATURE_C) {
    Serial.println("SHT30 read failed: temperature out of range.\n");
    return false;
  }

  if (humidityPercent < MIN_HUMIDITY_PERCENT ||
      humidityPercent > MAX_HUMIDITY_PERCENT) {
    Serial.println("SHT30 read failed: humidity out of range.\n");
    return false;
  }

  Serial.printf("Temperature: %.1f C\n", temperatureC);
  Serial.printf("Humidity: %.1f %%\n\n", humidityPercent);
  return true;
}

/**
 * 初始化 USB 串口、I²C 总线和 SHT30。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  sensorReady = sht30.begin(SHT30_ADDRESS);
  if (sensorReady) {
    Serial.println("SHT30 sensor test started at address 0x44.\n");
  } else {
    Serial.println("SHT30 initialization failed at address 0x44.");
    Serial.println("Check VCC, GND, SDA and SCL wiring.\n");
  }
}

/**
 * 每两秒执行一次温湿度采样。
 */
void loop() {
  static unsigned long sampleCount = 1;

  if (sensorReady) {
    readAndPrintMeasurement(sampleCount);
    sampleCount++;
  }
  delay(SAMPLE_INTERVAL_MS);
}
