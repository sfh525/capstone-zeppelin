#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

// Define LoRa pins
#define SS 5
#define RST 14
#define DIO0 2

// Define MQ-7 analog output pin
#define MQ7_AOUT_PIN 13

// MQ-7 calibration constants
#define RL 10000.0  // Load resistor in ohms
#define VCC 5.0     // Supply voltage
#define R0 72033.0  // Resistance in clean air
#define A 100.0     // Sensitivity curve constant for CO
#define B -1.5      // Sensitivity curve exponent for CO

// LoRa frequency and sync word
#define LORA_FREQUENCY 433E6
#define SYNC_WORD 0xF3
#define SENDER_ID "Sender1"

// GPS serial pins
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

// Timing variables
unsigned long previousMillis = 0;
const long interval = 2000;

// TinyGPSPlus object
TinyGPSPlus gps;

// HardwareSerial for GPS
HardwareSerial gpsSerial(2);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize MQ-7 AOUT pin
  pinMode(MQ7_AOUT_PIN, INPUT);
  analogReadResolution(12);

  // Initialize GPS serial
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);

  // Setup LoRa
  LoRa.setPins(SS, RST, DIO0);
  while (!LoRa.begin(LORA_FREQUENCY)) {
    delay(1000);
  }
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.enableCrc();
}

void loop() {
  // Update GPS data
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read()) && gps.location.isValid()) {
      break;
    }
  }

  unsigned long currentMillis = millis();

  // Send data every 10 seconds
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read MQ-7 AOUT with averaging
    const int samples = 10;
    long sum = 0;
    for (int i = 0; i < samples; i++) {
      sum += analogRead(MQ7_AOUT_PIN);
      delay(1);
    }
    int mq7Value = sum / samples;
    float voltage = mq7Value * (VCC / 4095.0);
    float rs = (voltage > 0) ? RL * ((VCC / voltage) - 1) : 0;
    float rs_r0 = (rs > 0) ? rs / R0 : 0;
    float ppm = (rs_r0 > 0) ? A * pow(rs_r0, B) : 0;

    // Get GPS data
    String latStr = gps.location.isValid() && gps.location.age() < 10000 && gps.satellites.value() >= 4 ? String(gps.location.lat(), 6) : "NoFix";
    String lngStr = gps.location.isValid() && gps.location.age() < 10000 && gps.satellites.value() >= 4 ? String(gps.location.lng(), 6) : "NoFix";

    // Create message
    String fullMessage = String(SENDER_ID) + ":PPM=" + String(ppm, 1) +
                        ",GPSLat=" + latStr + ",GPSLng=" + lngStr;

    // Send packet via LoRa
    LoRa.beginPacket();
    LoRa.print(fullMessage);
    LoRa.endPacket();

    // Print sent data and Google Maps URL
    Serial.print("Sent: ");
    Serial.println(fullMessage);
    if (latStr != "NoFix") {
      Serial.println("GPS Google Maps: https://www.google.com/maps?q=" + latStr + "," + lngStr);
    }
  }
}