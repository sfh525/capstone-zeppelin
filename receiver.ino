#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>

// Define the pins used by the LoRa transceiver module
#define SS 5
#define RST 14
#define DIO0 2

// LoRa frequency (must match senders: 433E6 for Asia)
#define LORA_FREQUENCY 433E6

// Sync words for two LoRa senders
#define SYNC_WORD_1 0xF3 // Sender 1
#define SYNC_WORD_2 0xF4 // Sender 2

// Wi-Fi credentials
const char* ssid = "iPhone";
const char* password = "12345678";

// Timer variables
unsigned long startTime = 0;
unsigned long lastPacketTime = 0;
bool timingStarted = false;

// Variables to store LoRa data for two senders
String lastPacketData1 = "No packet from Sender 1";
int lastPacketRSSI1 = 0;
String lastPacketData2 = "No packet from Sender 2";
int lastPacketRSSI2 = 0;
float elapsedTimeSeconds = 0.0;

// Create a web server on port 80
WebServer server(80);

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("LoRa Receiver for Two Senders with Real-Time Web Server");
  Serial.println("Type 'STOP' in Serial Monitor to end the timer and get final duration.");

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int wifiRetryCount = 0;
  const int maxWifiRetries = 20;
  while (WiFi.status() != WL_CONNECTED && wifiRetryCount < maxWifiRetries) {
    delay(500);
    Serial.print(".");
    wifiRetryCount++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed. Check credentials or signal.");
    while (true) delay(1000);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Web server started. Visit http://" + WiFi.localIP().toString());

  // Setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  // Initialize LoRa
  int loraRetryCount = 0;
  const int maxLoraRetries = 5;
  while (!LoRa.begin(LORA_FREQUENCY) && loraRetryCount < maxLoraRetries) {
    Serial.println("LoRa initialization failed. Retrying...");
    delay(1000);
    loraRetryCount++;
  }

  if (loraRetryCount >= maxLoraRetries) {
    Serial.println("LoRa initialization failed. Check connections and frequency.");
    while (true) delay(1000);
  }

  // Enable CRC for packet validation
  LoRa.enableCrc();
  Serial.println("LoRa Initialized OK!");
}

// Handle root URL request
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><title>ESP32 LoRa Receiver</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body { font-family: Arial, sans-serif; text-align: center; }";
  html += "h1 { color: #333; } .data { font-size: 1.2em; margin: 10px; }";
  html += ".packet { border: 1px solid #ccc; padding: 10px; margin: 5px; border-radius: 5px; }</style>";
  html += "</head><body>";
  html += "<h1>ESP32 LoRa Receiver (Two Senders)</h1>";
  html += "<div class='packet'><b>Sender 1 Packet:</b> <span id='packet1'>No packet from Sender 1</span></div>";
  html += "<div class='packet'><b>Sender 2 Packet:</b> <span id='packet2'>No packet from Sender 2</span></div>";
  html += "<div class='data'><b>Elapsed Time:</b> <span id='time'>0.00</span> seconds</div>";
  html += "<script>";
  html += "function updateData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('packet1').innerText = data.packet1;";
  html += "    document.getElementById('packet2').innerText = data.packet2;";
  html += "    document.getElementById('time').innerText = data.time.toFixed(2);";
  html += "  }).catch(err => console.error('Error fetching data:', err));";
  html += "}";
  html += "setInterval(updateData, 1000);"; // Update every 1 second
  html += "updateData();"; // Initial update
  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Handle data endpoint for AJAX requests
void handleData() {
  String json = "{";
  json += "\"packet1\":\"" + lastPacketData1 + "\",";
  json += "\"packet2\":\"" + lastPacketData2 + "\",";
  json += "\"time\":" + String(elapsedTimeSeconds, 2);
  json += "}";
  server.send(200, "application/json", json);
}

// Function to check for packets with a specific sync word
bool checkForPacket(uint8_t syncWord, String& packetData, int& packetRSSI) {
  LoRa.setSyncWord(syncWord);
  unsigned long startCheck = millis();
  const unsigned long timeout = 50; // 50ms for fast polling

  // Clear LoRa buffer
  while (LoRa.available()) LoRa.read();

  while (millis() - startCheck < timeout) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      char packetBuffer[256];
      int index = 0;
      while (LoRa.available() && index < sizeof(packetBuffer) - 1) {
        packetBuffer[index++] = (char)LoRa.read();
      }
      packetBuffer[index] = '\0';

      packetData = String(packetBuffer);
      packetRSSI = LoRa.packetRssi();

      // Log all packets for debugging
      Serial.print("Received packet with sync word 0x");
      Serial.print(syncWord, HEX);
      Serial.print(": '");
      Serial.print(packetData);
      Serial.print("' with RSSI ");
      Serial.println(packetRSSI);

      // Validate packet prefix
      if (syncWord == SYNC_WORD_1 && !packetData.startsWith("Sender1:")) {
        Serial.println("Warning: Sender 1 packet has incorrect prefix: " + packetData);
        return false;
      }
      if (syncWord == SYNC_WORD_2 && !packetData.startsWith("Sender2:")) {
        Serial.println("Warning: Sender 2 packet has incorrect prefix: " + packetData);
        return false;
      }

      return true;
    }
  }
  return false;
}

void loop() {
  // Handle web server requests
  server.handleClient();

  // Check for Serial input to stop the timer
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("STOP") && timingStarted) {
      unsigned long totalTime = lastPacketTime - startTime;
      Serial.print("Timer stopped. Total transmission time: ");
      Serial.print(totalTime / 1000.0);
      Serial.println(" seconds");
      timingStarted = false;
      lastPacketData1 = "Timer stopped (Sender 1)";
      lastPacketData2 = "Timer stopped (Sender 2)";
      elapsedTimeSeconds = totalTime / 1000.0;
      return;
    }
  }

  // Check for packets from Sender 1
  String packetData;
  int packetRSSI;
  if (checkForPacket(SYNC_WORD_1, packetData, packetRSSI)) {
    if (!timingStarted) {
      startTime = millis();
      timingStarted = true;
      Serial.println("Timer started!");
    }

    lastPacketTime = millis();
    elapsedTimeSeconds = (lastPacketTime - startTime) / 1000.0;

    lastPacketData1 = packetData;
    lastPacketRSSI1 = packetRSSI;

    Serial.print("Accepted packet from Sender 1 '");
    Serial.print(lastPacketData1);
    Serial.print("' with RSSI ");
    Serial.print(lastPacketRSSI1);
    Serial.print(" | Elapsed time: ");
    Serial.print(elapsedTimeSeconds);
    Serial.println(" seconds");
  }

  // Check for packets from Sender 2
  if (checkForPacket(SYNC_WORD_2, packetData, packetRSSI)) {
    if (!timingStarted) {
      startTime = millis();
      timingStarted = true;
      Serial.println("Timer started!");
    }

    lastPacketTime = millis();
    elapsedTimeSeconds = (lastPacketTime - startTime) / 1000.0;

    lastPacketData2 = packetData;
    lastPacketRSSI2 = packetRSSI;

    Serial.print("Accepted packet from Sender 2 '");
    Serial.print(lastPacketData2);
    Serial.print("' with RSSI ");
    Serial.print(lastPacketRSSI2);
    Serial.print(" | Elapsed time: ");
    Serial.print(elapsedTimeSeconds);
    Serial.println(" seconds");
  }

  // Small delay to prevent tight looping
  delay(10);
}