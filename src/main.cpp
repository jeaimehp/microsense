/****************************
 * Project: µSense
 * By: Je'aime Powell
 * Purpose: Micro-climate monitoring device
 * that uses a BME680 for sensing and Lora communications
 * Created: 6/20/20 for Capsule Hackathon
 * Tested device: Heltech ESP32 WiFi Lora 32 (V2)
 */

//Libraries
#include "Arduino.h"
#include "heltec.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <lmic.h>
#include <hal/hal.h>
#include <lmic/lmic_util.h>

// The Things Network Application Device info:
// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={ PUT YOURS IN LSB };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]={ PUT YOURS IN LSB };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t PROGMEM APPKEY[16] = { PUT YOURS IN MSB };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}



//BME680 (0x77)
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme; // I2C


///////// The Things Network (TTN) Lora Setup and Functions /////////////
static osjob_t sendjob;
static uint8_t payload[8];
static uint8_t mydata[] = "Hello, World";

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26,35,34},
};

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        
 
        // prepare upstream data transmission at the next possible time.
        // transmit on port 1 (the first parameter); you can use any value from 1 to 223 (others are reserved).
        // don't request an ack (the last parameter, if not zero, requests an ack from the network).
        // Remember, acks consume a lot of network resources; don't ask for an ack unless you really need it.

        //BME680 Reading and conversion from float to int to byte in prep for TX to TTN
        if (! bme.performReading()) {
         Serial.println("Failed to perform reading :(");
         return;
         }
         uint32_t temperature = bme.temperature * 100;
         uint32_t humidity = bme.humidity * 100;
         uint32_t pressure = bme.pressure * 100;
         uint32_t gas = (bme.gas_resistance / 1000.0) * 100;

         delay(500);
         
         // Oled Update
          Heltec.display->clear();
          Heltec.display->setFont(ArialMT_Plain_24);
          Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
          Heltec.display->drawString(20, 0, "µSense");
          Heltec.display->setFont(ArialMT_Plain_10);
          Heltec.display->drawString(0, 25, "Temperature: " + String(bme.temperature) + " °C");
          Heltec.display->drawString(0, 35, "Humidity: " + String(bme.humidity) + " %");
          Heltec.display->drawString(0, 45, "Gas: " + String(bme.gas_resistance / 1000.0) + " KOhms");
          Heltec.display->display();         

         //set the readings into 2 bytes
         payload[0] = highByte(temperature);
         payload[1] = lowByte(temperature);
         payload[2] = highByte(humidity);
         payload[3] = lowByte(humidity);
         payload[4] = highByte(pressure);
         payload[5] = lowByte(pressure);
         payload[6] = highByte(gas);
         payload[7] = lowByte(gas);

         /******
          * Example payload = 0A76171E42CC00
          * 
          * Payload Format Code Examples
          * 
          * //To Decode the Hello, World Payload on TTN
          * function Decoder(bytes, port) {
          *   // Decode plain text; for testing only 
          *      return {
          *          data: String.fromCharCode.apply(null, bytes)
          *       };
          *       }
          * 
          * // To decode the BME680 data payload on TTN
          * 
          * function Decoder(bytes, port) {
          *   var temperature = (bytes[0] << 8) | bytes[1];
          *   var humidity = (bytes[2] << 8) | bytes[3];
          *   var pressure = (bytes[4] << 8) | bytes[5];
          *   var gas = (bytes[6] << 8) | bytes[7];
          *
          *   return {
          *     temperature: temperature /100,
          *     humidity: humidity /100,
          *     pressure: pressure /100,
          *     gas: gas / 100
          *   }
          *
          * }
          * 
          * 
          */

         
         
        // Send Lora TX to TTN       
        LMIC_setTxData2(1, payload, sizeof(payload)-1, 0);
        
       //Test "Hello, World" Payload 
       //LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);

       
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));

            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}



//////// Setup Function /////////////
void setup() {
  // heltec display and lora initiation
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, false /*Serial Enable*/);

  // Lora connection initiation
  SPI.begin(5, 19, 27);
  

  //Heltec.display->flipScreenVertically();
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_24);
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->drawString(20, 0, "µSense");
  Heltec.display->display();


  // Serial monitor initiation
  Serial.begin(9600); // Sets the speed needed to read from the serial port when connected
  while (!Serial); // Loop that only continues once the serial port is active (true)
  Serial.println(F("µSense entering Setup"));



  // BME680 Initiation
  Serial.println("Starting BME680 on I2C");
  if (!bme.begin()) {  // Attempts to initialize the BME680 and it not found prints below message until found
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    Serial.println("Pins on Heltec ESP32 Wifi Lora 32 V2 SCL=15 SDA=4");
    while (1); // Continues in loop until BME680 not found corrected (requires reset)
  }

  // LMIC init (TTN)
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);


}


//////// Loop Function /////////////
void loop() {
   os_runloop_once(); // Readings taken, sent and looped in the do_send() function called from setup
  
}
