#include <Arduino.h>
#include "BluetoothSerial.h"
#include "NDEF.h"
#include <SPI.h>
#include <MFRC522.h>
#include "FastLED.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

const String DEVICE_NAME = "ESP32_CANAM8";
BluetoothSerial SerialBT;

// MFRC522 Configuration
#define RST_PIN    22   //
#define SS_PIN     21    //
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::StatusCode status;

// LED Variables
bool LED_STATUS_ON_PREV = false;
bool LED_STATUS_ON = false;
int color_R = 255;
int color_G = 255;
int color_B = 255;
int danceArr1[4] = {2, 7, 15, 20};
const int dinPin = 12;    // Din of led strip
const int numOfLeds = 24; // Number of leds
CRGBArray<numOfLeds> leds;

// proximity tracker flags
bool rfid_tag_present_prev = false;
bool rfid_tag_present = false;
int _rfid_error_counter = 0;
bool _tag_found = false;

// Functions and Thread declarations
TaskHandle_t LEDTask;
void longToByteArray(long inLong, byte* outArray);
void LEDThread(void * parameters);
void animateQuadRotate();
void turnOnLED();
void turnOffLED();

void setup() {
  Serial.begin(115200);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  FastLED.addLeds<WS2811, dinPin, GRB>(leds, numOfLeds).setCorrection(TypicalSMD5050).setTemperature(Halogen);
  SerialBT.begin(DEVICE_NAME); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");

  xTaskCreatePinnedToCore(
    LEDThread,  /* Task function. */
    "LEDTask",    /* name of task. */
    10000,      /* Stack size of task */
    NULL,       /* parameter of the task */
    1,          /* priority of the task */
    &LEDTask,     /* Task handle to keep track of created task */
    0           /* pin task to core 0, since arduino sketches runs on core 1 */
  );         
}

void loop() {

  rfid_tag_present_prev = rfid_tag_present;

  _rfid_error_counter += 1;
  if(_rfid_error_counter > 2){
    _tag_found = false;
  }

  // Detect Tag without looking for collisions
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);

  // Reset baud rates
  mfrc522.PCD_WriteRegister(mfrc522.TxModeReg, 0x00);
  mfrc522.PCD_WriteRegister(mfrc522.RxModeReg, 0x00);

  // Reset ModWidthReg
  mfrc522.PCD_WriteRegister(mfrc522.ModWidthReg, 0x26);

  MFRC522::StatusCode result = mfrc522.PICC_RequestA(bufferATQA, &bufferSize);

  if(result == mfrc522.STATUS_OK){
    if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue   
      return;
    }
    _rfid_error_counter = 0;
    _tag_found = true;        
  }
  
  rfid_tag_present = _tag_found;

  // when we detect a new NFC in proximity
  if (rfid_tag_present && !rfid_tag_present_prev) {

    Serial.println("Tag found");
    Serial.println(F("Reading data ... "));

    int block;
    byte buf[4];

    byte res[64];
    byte index = 0;

    for(int i=4; i<16; i++)
    {
      mfrc522.MIFARE_GetValue(i, &block);
      longToByteArray(block, buf);
      memcpy(res+index, buf, 4);
      index += 4;
    }

    FOUND_MESSAGE msg = NDEF().decode_message((uint8_t*)res);

    String ndefUri((char*)msg.payload);
    Serial.println("NDEF URI: " + ndefUri);

    // send data to device via bluetooth
    SerialBT.print("event_rfid|data_" + ndefUri);
  }

  // when NFC is not in proximity
  if (!rfid_tag_present && rfid_tag_present_prev) {

    Serial.println("Tag gone");
    SerialBT.print("event_rfid-detach|data_null");

    // turn OFF LED
    if(LED_STATUS_ON) {
      LED_STATUS_ON = false;
    }
  }

  // if there incoming data from BT, let's process it
  if(SerialBT.available()) {
    String rgb = SerialBT.readString();
    Serial.println("RGB: " + rgb);

    String batch = "";
    int rCounter = 0;
    String items[3];
    std::string str2(rgb.c_str());
    for (char & c : str2){

      if(c != ',') {
        batch += c;
      } else {
        items[rCounter] = batch;
        batch = "";
        rCounter++;
      }
    }

    items[rCounter] = batch;
    Serial.println("R: " + items[0]);
    Serial.println("G: " + items[1]);
    Serial.println("B: " + items[2]);
    color_R = items[0].toInt();
    color_G = items[1].toInt();
    color_B = items[2].toInt();

    // turn ON LED
    if(!LED_STATUS_ON) {
      Serial.println("Turn ON LED.");
      LED_STATUS_ON = true;
    }
  }
}

void longToByteArray(long inLong, byte* outArray)
{
  outArray[0] = inLong;
  outArray[1] = (inLong >> 8);
  outArray[2] = (inLong >> 16);
  outArray[3] = (inLong >> 24);
}

void LEDThread(void * parameters) {

  while(1) {

    // turn on
    if(LED_STATUS_ON && (LED_STATUS_ON != LED_STATUS_ON_PREV)) {
      LED_STATUS_ON_PREV = LED_STATUS_ON;
      Serial.print("ONNNNN");
      for(int i=0;i<numOfLeds;i++)
      {
        bool exists = std::find(std::begin(danceArr1), std::end(danceArr1), i) != std::end(danceArr1);
        if(exists) {
          leds[i] = CRGB::White;
        } else {
          leds[i].setRGB(color_R,color_G,color_B);
        }
        FastLED.show();
        delay(25);
      }
    }

    // turn off
    if(!LED_STATUS_ON && (LED_STATUS_ON != LED_STATUS_ON_PREV)) {
      LED_STATUS_ON_PREV = LED_STATUS_ON;
      Serial.print("OFFFFFF");
      for(int j=0;j<255;j++) {
        if(color_R > 0){ color_R--; }
        if(color_G > 0){ color_G--; }
        if(color_B > 0){ color_B--; }
        for(int i=0;i<numOfLeds;i++) {
          leds[i].setRGB(color_R,color_G,color_B);
        }
        FastLED.show();
        delay(5);
      }
    }

    if(LED_STATUS_ON) {
      if(danceArr1[0] == numOfLeds){ danceArr1[0] = 1; } else { danceArr1[0] = danceArr1[0] + 1; }
      if(danceArr1[1] == numOfLeds){ danceArr1[1] = 1; } else { danceArr1[1] = danceArr1[1] + 1; }
      if(danceArr1[2] == numOfLeds){ danceArr1[2] = 1; } else { danceArr1[2] = danceArr1[2] + 1; }
      if(danceArr1[3] == numOfLeds){ danceArr1[3] = 1; } else { danceArr1[3] = danceArr1[3] + 1; }
      for(int i=0;i<numOfLeds;i++) {
        bool exists = std::find(std::begin(danceArr1), std::end(danceArr1), i) != std::end(danceArr1);
        if(exists) {
          leds[i] = CRGB::White;
        } else {
          leds[i].setRGB(color_R,color_G,color_B);
        }
        FastLED.show();
        delay(2);
      }
    }

    delay(50);
  }
}