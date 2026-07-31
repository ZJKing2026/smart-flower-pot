#include <Wire.h>
#include <BH1750.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SAMPLE_INTERVAL_MS = 2000;
constexpr float SATURATION_LEVEL_LUX = 65535.0F;

BH1750 lightMeter;
bool sensorReady = false;

/**
 * 读取并校验一组照度数据，然后输出到串口。
 *
 * @param sampleCount 当前采样编号。
 * @return 数据有效时返回 true，否则返回 false。
 */
bool readAndPrintLightLevel(unsigned long sampleCount) {
  const float lightLevelLux = lightMeter.readLightLevel();

  Serial.printf("Sample #%lu\n", sampleCount);
  if (isnan(lightLevelLux) || lightLevelLux < 0.0F) {
    Serial.println("BH1750 read failed: invalid value received.\n");
    return false;
  }

  Serial.printf("Light: %.1f lx\n", lightLevelLux);
  if (lightLevelLux >= SATURATION_LEVEL_LUX) {
    Serial.println("Warning: sensor may be saturated.");
  }
  Serial.println();
  return true;
}

/**
 * 初始化 USB 串口、I²C 总线和 BH1750。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  sensorReady = lightMeter.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    BH1750_ADDRESS,
    &Wire
  );
  if (sensorReady) {
    Serial.println("BH1750 sensor test started at address 0x23.\n");
  } else {
    Serial.println("BH1750 initialization failed at address 0x23.");
    Serial.println("Check VCC, GND, SDA, SCL and ADD wiring.\n");
  }
}

/**
 * 每两秒执行一次照度采样。
 */
void loop() {
  static unsigned long sampleCount = 1;

  if (sensorReady) {
    readAndPrintLightLevel(sampleCount);
    sampleCount++;
  }
  delay(SAMPLE_INTERVAL_MS);
}
