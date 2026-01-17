#include <Arduino.h>
#include "RotationManager.h"

#include <FastLED.h>

#define LED_PIN 46
#define COLOR_ORDER GRB
#define CHIPSET WS2811
#define NUM_LEDS 1
#define BRIGHTNESS 250

CRGB leds[NUM_LEDS];

static const int INT_PIN = 10; // (INT pin on MLX90393)

MLX90393_Configurable mlx;
MLX90393ArduinoHal arduinoHal;

volatile bool interruptTriggered = false;
unsigned long lastInterruptTime = 0;

void IRAM_ATTR dataReadyISR()
{
  interruptTriggered = true;
  lastInterruptTime = millis();
}

void enterWakeOnChangeMode()
{
  Serial.println("Entering Wake-On-Change mode...");
  mlx.exit();
  uint8_t wocDiff;
  mlx.getWocDiff(wocDiff);
  Serial.printf("Current WOC_DIFF setting: %d\n", wocDiff);
  if (wocDiff != 1)
  {
    Serial.println("Setting WOC_DIFF to 1 for Wake-On-Change mode.");
    mlx.setWocDiff(1); // Enable Wake-On-Change on difference
  }
  mlx.setWOXYThreshold(10); // Set threshold for wake-on-change
  mlx.startWakeOnChange(MLX90393::X_FLAG | MLX90393::Y_FLAG);
  delay(10);
  Serial.println("Entered Wake-On-Change mode.");
  esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_PIN, HIGH);
  esp_deep_sleep_start();
}

void setup()
{
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset

  // delay(5000);
  Serial.begin(115200);
  Serial.println("Starting up...");
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH); // enable peripheral power

  // Wire1.setPins(8, 9);
  Wire1.begin(8, 9, 100000); // SDA, SCL, frequency

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  // if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
  //   Serial.println("Woke up from Magenetometer Wake-On-Change interrupt.");
  arduinoHal.set_twoWire(&Wire1);

  attachInterrupt(digitalPinToInterrupt(INT_PIN), dataReadyISR, RISING);

  uint8_t status = mlx.begin_with_hal(&arduinoHal); // A1, A0
  mlx.reset();

  mlx.setGainSel(1);
  mlx.setResolution(17, 17, 16); // x, y, z
  mlx.setOverSampling(2);
  mlx.setDigitalFiltering(4);
  mlx.startBurst(MLX90393::X_FLAG | MLX90393::Y_FLAG);
}

bool hasStartedWakeOnChange = false;
void loop()
{
  if (millis() > 4000 && !hasStartedWakeOnChange)
  {
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(500);
    enterWakeOnChangeMode();
    hasStartedWakeOnChange = true;
  }

  if (interruptTriggered && (millis() - lastInterruptTime < 500))
  {
    Serial.println("Magnetometer interrupt triggered!");
    // Flash LED blue briefly
    leds[0] = CRGB::Blue;
    FastLED.show();
    delay(250);
    leds[0] = CRGB::Black;
    FastLED.show();
    interruptTriggered = false;
  }
  MLX90393::txyzRaw data; // Structure to hold x, y, z data

  // Read the magnetometer data (this should be fast in burst mode)
  if (mlx.readMeasurement(MLX90393::X_FLAG | MLX90393::Y_FLAG, data))
  {
    int16_t x = static_cast<int16_t>(data.x);
    int16_t y = static_cast<int16_t>(data.y);
    float angle = atan2(-y, x);
    angle = angle * 180 / PI; // convert to degrees
    angle += 180;             // offset to 0-360 degrees

    Serial.printf("X: %d, Y: %d, Angle: %.2f\n", x, y, angle);
  }
}
