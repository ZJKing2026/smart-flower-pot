constexpr uint8_t SOIL_SENSOR_PIN = 1;
constexpr uint8_t ADC_RESOLUTION_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint8_t SAMPLE_COUNT = 20;
constexpr unsigned long SAMPLE_DELAY_MS = 5;
constexpr unsigned long OUTPUT_INTERVAL_MS = 2000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;

struct AdcStatistics {
  uint16_t average;
  uint16_t minimum;
  uint16_t maximum;
  uint32_t millivolts;
};

/**
 * 连续采集土壤传感器 ADC，并计算统计值。
 *
 * @return 本轮 ADC 平均值、最小值、最大值和近似毫伏值。
 */
AdcStatistics sampleSoilSensor() {
  uint32_t total = 0;
  uint16_t minimum = ADC_MAX_VALUE;
  uint16_t maximum = 0;

  for (uint8_t index = 0; index < SAMPLE_COUNT; index++) {
    const uint16_t rawValue = analogRead(SOIL_SENSOR_PIN);
    total += rawValue;
    minimum = min(minimum, rawValue);
    maximum = max(maximum, rawValue);
    delay(SAMPLE_DELAY_MS);
  }

  AdcStatistics statistics;
  statistics.average = total / SAMPLE_COUNT;
  statistics.minimum = minimum;
  statistics.maximum = maximum;
  statistics.millivolts = analogReadMilliVolts(SOIL_SENSOR_PIN);
  return statistics;
}

/**
 * 初始化 USB 串口和 GPIO1 ADC 参数。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);
  Serial.println("Soil moisture raw ADC test started.\n");
}

/**
 * 每两秒采集并输出一组 ADC 统计数据。
 */
void loop() {
  static unsigned long sampleCount = 1;
  const AdcStatistics statistics = sampleSoilSensor();

  Serial.printf("Sample #%lu\n", sampleCount);
  Serial.printf("ADC average: %u\n", statistics.average);
  Serial.printf(
    "ADC min/max: %u / %u\n",
    statistics.minimum,
    statistics.maximum
  );
  Serial.printf("Voltage: %lu mV\n\n", statistics.millivolts);

  sampleCount++;
  delay(OUTPUT_INTERVAL_MS);
}
