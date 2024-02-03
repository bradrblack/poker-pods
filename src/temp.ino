/* Poker Pods - (C) Brad Black 2024
 - Simple Remote Display for a Poker Blind Timer - https://github.com/bradrblack/MAX72XX-LED-Poker-Blind-Timer
 - Receives blind levels over ESP-NOW protocol and displays on LilyGo T-Display S3
 - "Top" Button will put the device to sleep or wake from sleep
 - "Bottom" Button will change background color & write to EEPROM for next wakeup
 */

#include "EEPROM.h"
#include "TFT_eSPI.h" // Hardware-specific library
#include <SPI.h>
#include "Free_Fonts.h" // Include the header file attached to this sketc
#include "SDGothicNeoBold24.h"
#include "SDGothicNeoBold72.h"
#include "SDGothicNeoBold96.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "WiFi.h"

#ifdef TDISP
#define height 135
#define width 240
#define BIGFONT FF23
#define TEMPFONT SDGothicNeoBold72
#define getVoltage getVoltageNonS3
#define SLEEP_PIN 35
#endif

#ifdef TDISPS3
#define height 170
#define width 320
#define BIGFONT FF24
#define TEMPFONT SDGothicNeoBold72
#define getVoltage getVoltageS3
#define SLEEP_PIN 14
#define PIN_COLOR_CHANGE 0
#endif

#define BATTERY true // SET true to display battery voltage
#define PIN_BAT_VOLT 4
#define PIN_POWER_ON 15

#define ADC_EN 14 // ADC_EN is the ADC detection enable port
#define ADC_PIN 34

int background[] = {0x0000, 0x000F, 0x03E0, 0x03EF, 0x7800, 0x780F, 0x7BE0};
int currentBackground = 0;
const int numColors = 6;

/*
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
*/

int vref = 1100;
float v;
float voltage = 0;
int timer;

uint32_t start;

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

unsigned long drawTime = 0;

char tempDisplay[20];

typedef struct msg
{
  int small;
  int big;
} msg;

// Create a struct_message called blindData
msg blindData;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{

  int thousands = 0;

  memcpy(&blindData, incomingData, sizeof(blindData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Small Blind: ");
  Serial.println(blindData.small);
  Serial.print("Big Blind: ");
  Serial.println(blindData.big);
  Serial.println();
  showBlinds();
}

void showBlinds()
{
  tft.loadFont(TEMPFONT);
  tft.setTextColor(TFT_YELLOW, background[currentBackground], true);
  tft.setTextPadding(width);

  if (blindData.small < 1000)
    sprintf(tempDisplay, "%d", blindData.small);
  else
  {
    sprintf(tempDisplay, "%d,%03d", blindData.small / 1000, blindData.small % 1000);
  }

  tft.setTextDatum(TL_DATUM);
  tft.drawString(tempDisplay, 16, 40);
  if (blindData.big < 1000)
    sprintf(tempDisplay, "%d", blindData.big);
  else
  {
    sprintf(tempDisplay, "%d,%03d", blindData.big / 1000, blindData.big % 1000);
  };

  tft.setTextDatum(TR_DATUM);
  tft.drawString(tempDisplay, width - 20, 104);
}
void updateVoltage()
{

  v = getVoltage();
  delay(100);

  if (BATTERY)
  {
    if (v > 4.3)
    {
      tft.setTextColor(TFT_RED, background[currentBackground], true);
      tft.loadFont(SDGothicNeoBold24);
      tft.setTextDatum(TR_DATUM);
      tft.drawString("    ", width - 20, 10);
      tft.drawString("CHRG", width - 20, 10);
    }
    else
    {
      tft.setTextColor(TFT_WHITE, background[currentBackground], true);
      tft.setTextColor(TFT_WHITE, background[currentBackground], true);
      tft.loadFont(SDGothicNeoBold24);
      tft.setTextDatum(TR_DATUM);
      tft.setTextPadding(0);
      tft.drawString("         ", width - 35, 10);
      tft.drawFloat(v, 1, width - 35, 10);
      tft.drawString("V", width - 20, 10);
    }
  }
  tft.setTextColor(TFT_WHITE, background[currentBackground], true);
  tft.loadFont(SDGothicNeoBold24);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
  tft.drawString("Blinds", 20, 10);
}

float getVoltageS3()
{
  float volts = (analogRead(PIN_BAT_VOLT) * 2 * 3.3) / 4096;
  Serial.printf("volts: %f\n", volts);
  return volts;
}

float getVoltageNonS3()
{
  float battery_voltage;
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 500)
  {
    timeStamp = millis();
    uint16_t v = analogRead(ADC_PIN);
    Serial.print(v);
    Serial.println(" is the value of ADC_PIN");
    battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    String voltage = "Voltage :" + String(battery_voltage) + "V";
    Serial.println(voltage);
  }
  else
    battery_voltage = -1;

  return battery_voltage;
}

void setup(void)
{
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(SLEEP_PIN, INPUT);
  pinMode(PIN_COLOR_CHANGE, INPUT);

  start = millis();

  Serial.begin(115200);
  delay(200);
  EEPROM.begin(32);

  currentBackground = EEPROM.read(0);
  Serial.printf("Read color value from EEPROM: %d\n", currentBackground);
  if ((currentBackground < 0) || (currentBackground > numColors))
    currentBackground = 0;

  tft.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  delay(500);
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info

  Serial.println("ESP-NOW-INIT complete");
  esp_now_register_recv_cb(OnDataRecv);

  tft.setRotation(1);
  tft.writecommand(0x21);
  tft.fillScreen(background[currentBackground]); // Clear screen to CURRENT background
  tft.loadFont(TEMPFONT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, background[currentBackground], true);
  tft.drawString("Hello.", width / 2, (height / 2) + 2);
  tft.unloadFont();
  tft.loadFont(SDGothicNeoBold24);

  updateVoltage();
}

void loop()
{

  if (!digitalRead(SLEEP_PIN))
  {
    Serial.println("Going to sleep now");
    tft.fillScreen(background[currentBackground]); // Clear screen to CURRENT background
    delay(500);
    digitalWrite(PIN_POWER_ON, LOW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SLEEP_PIN, 0);
    esp_deep_sleep_start();
  }
  if (millis() - timer > 30000)
  {
    timer = millis();
    tft.setTextColor(TFT_YELLOW, background[currentBackground], true);
    tft.loadFont(TEMPFONT);

    updateVoltage();
  }
  if (!digitalRead(PIN_COLOR_CHANGE))
  {
    Serial.println("Color Change");
    if (currentBackground < numColors)
      currentBackground++;
    else
      currentBackground = 0;
    tft.fillScreen(background[currentBackground]);
    updateVoltage();
    showBlinds();
    Serial.printf("Writing value to EEPROM: %d\n", currentBackground);
    EEPROM.write(0, currentBackground);
    EEPROM.commit();
    delay(500);
  }
}
