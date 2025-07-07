#include <SPI.h>
#include <LoRa.h>

// Define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

// Define MQ-7 analog output pin
#define MQ7_AOUT_PIN 13

// MQ-7 calibration constants
#define RL 10000.0  // Load resistor in ohms (check module schematic)
#define VCC 5.0     // Supply voltage
#define R0 72033.0  // Resistance in clean air (CALIBRATE THIS!)
#define A 100.0     // Sensitivity curve constant for CO
#define B -1.5      // Sensitivity curve exponent for CO

// Variables for timing
unsigned long previousMillis = 0;
const long interval = 10000; // Send data every 500ms
unsigned long loopCount = 0; // Debug counter

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Slave 1 - Sending Real-Time MQ-7 Analog Data with PPM");

  // Initialize MQ-7 AOUT pin
  pinMode(MQ7_AOUT_PIN, INPUT);

  // Set ADC resolution to 12-bit (0-4095)
  analogReadResolution(12);

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);
  
  // Initialize LoRa with 433 MHz
  while (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Retrying...");
    delay(100);
  }
  
  // Set sync word to match master
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");

  // Preheat MQ-7
  Serial.println("Preheating MQ-7 for 20 seconds...");
  delay(20000);
  Serial.println("Preheating complete!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Send data every 500ms
  if (currentMillis - previousMillis >= interval) {
    // Save the last time we sent data
    previousMillis = currentMillis;
    loopCount++;

    // Read MQ-7 AOUT pin with averaging to reduce noise
    const int samples = 10;
    long sum = 0;
    for (int i = 0; i < samples; i++) {
      sum += analogRead(MQ7_AOUT_PIN);
      delay(1);
    }
    int mq7Value = sum / samples;
    float voltage = mq7Value * (VCC / 4095.0);

    // Calculate Rs
    float rs = RL * ((VCC / voltage) - 1);

    // Calculate Rs/R0
    float rs_r0 = rs / R0;

    // Estimate CO ppm
    float ppm = A * pow(rs_r0, B);

    // Create message with sensor data
    String fullMessage = "Sender1:MQ7=" + String(mq7Value) + ",V=" + String(voltage, 2) + "V,PPM=" + String(ppm, 1);

    // Send packet via LoRa
    LoRa.beginPacket();
    LoRa.print(fullMessage);
    LoRa.endPacket();
    
    // Print sent data to Serial Monitor
    Serial.print("Loop ");
    Serial.print(loopCount);
    Serial.print(": Sent packet: '");
    Serial.print(fullMessage);
    Serial.println("'");
  }
}