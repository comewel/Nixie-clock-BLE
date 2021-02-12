#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>

#include "RTClib.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define NIXIE_CLOCK_SERVICE_UUID "USE ^ AND FILL HERE"

#define A1 15
#define B_1 4 // B1 is already defined
#define C1 16
#define D1 2

#define A2 17
#define B2 18
#define C2 19
#define D2 5

#define A3 32
#define B3 25
#define C3 26
#define D3 33

#define A4 27
#define B4 12
#define C4 13
#define D4 14

#define HOUR_ONE 2
#define HOUR_TWO 3

#define MINUTES_ONE 0
#define MINUTES_TWO 1

uint8_t nixie_map[4][4] = {
    {A1, B_1, C1, D1},
    {A2, B2, C2, D2},
    {A3, B3, C3, D3},
    {A4, B4, C4, D4},
};

BLECharacteristic *nixieCharacteristic;

// RTC DS3231
RTC_DS3231 rtc;

// BLUETOOTH
// ###########################################################################################

class NixieServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("connect");
  };

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("disconnect");
  }
};

class ManageClockCharacteristic : public BLECharacteristicCallbacks
{
  // expect string like hh:mm

  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    uint8_t hour;
    uint8_t minutes;

    std::string new_time = pCharacteristic->getValue();
    Serial.print("Received: ");
    Serial.println(new_time.c_str());

    uint32_t res = sscanf(new_time.c_str(), "%hhu:%hhu", &hour, &minutes);

    if (res != 2 || hour > 24 || minutes > 60)
    {
      return;
    }
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), hour, minutes));

    Serial.println("New time\t");
    Serial.print(hour);
    Serial.print(":");
    Serial.println(minutes);
  }

  void onRead(BLECharacteristic *pCharacteristic) override
  {
    Serial.println("reading value from service");
    DateTime now = rtc.now();

    char *time_now;
    int32_t res = asprintf(&time_now, "%hhu:%hhu", now.hour(), now.minute());
    if (res == -1)
    {
      Serial.println("Error allocating the space for the current time...");
      return;
    }

    pCharacteristic->setValue(time_now);
    pCharacteristic->notify();
    free(time_now);
  }
};

void init_BLE()
{
  Serial.println("Starting BLE server!");

  BLEDevice::init("Nixie-clock");
  BLEServer *nixieServer = BLEDevice::createServer();
  nixieServer->setCallbacks(new NixieServerCallbacks()); // maybe usefull?

  BLEService *pService = nixieServer->createService(NIXIE_CLOCK_SERVICE_UUID);

  nixieCharacteristic = pService->createCharacteristic(
      NIXIE_CLOCK_SERVICE_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);

  nixieCharacteristic->setCallbacks(new ManageClockCharacteristic());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NIXIE_CLOCK_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();

  Serial.println("Characteristic defined! Now you can read and write it from your phone!");
}

// ###########################################################################################

// RTC
void init_RTC()
{
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// ###########################################################################################

void set_nixie_value(uint8_t nixie_position, uint8_t value)
{
  uint8_t *nixie_cur = nixie_map[nixie_position];
  
  for (uint8_t nixie_pin = 0; nixie_pin < 4; nixie_pin++)
  {
    uint8_t current_multiplexer_value = (value >> nixie_pin) & 0x01;
    digitalWrite(nixie_cur[nixie_pin], current_multiplexer_value);
  }
}

void init_Nixie()
{
  for (uint8_t nixie_pos = 0; nixie_pos < 4; nixie_pos++)
  { // only 4 nixie :)
    for (uint8_t nixie_pin = 0; nixie_pin < 4; nixie_pin++)
    { // 4 input per multiplexer
      pinMode(nixie_map[nixie_pos][nixie_pin], OUTPUT);
      digitalWrite(nixie_map[nixie_pos][nixie_pin], LOW);
    }
  }
}
void test_nixie()
{
  uint8_t value_to_show = 0;
  while (true)
  {
    for (uint8_t nixie_pos = 0; nixie_pos < 4; nixie_pos++)
    {
      set_nixie_value(nixie_pos, value_to_show % 10);
    }
    value_to_show = (1 + value_to_show) % 10;
    delay(500);
  }
}
// ###########################################################################################
void setup()
{
  Serial.begin(57600);

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  init_RTC();
  init_BLE();

  init_Nixie();

  // use test_nixie only to check the integrity of the nixie tubes
  // test_nixie();
}

void loop()
{
  DateTime now = rtc.now();

  uint8_t hour = now.hour();
  uint8_t minutes = now.minute();

  set_nixie_value(HOUR_ONE, hour / 10);
  set_nixie_value(HOUR_TWO, hour % 10);

  set_nixie_value(MINUTES_ONE, minutes / 10);
  set_nixie_value(MINUTES_TWO, minutes % 10);

  delay(500);
}