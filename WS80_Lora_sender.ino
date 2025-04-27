#include <SPI.h>
#include <LoRa.h>

// LoRa Configuration
#define SCK     5    // LoRa SCK Pin
#define MISO    19   // LoRa MISO Pin
#define MOSI    27   // LoRa MOSI Pin
#define SS      18   // LoRa SS Pin
#define RST     23   // LoRa RST Pin
#define DI0     26   // LoRa DIO0 Pin
#define BAND    868E6 // LoRa frequency (868 MHz)

// Struct to store parsed data
struct SensorData {
  int windDir = 0;
  float windSpeed = 0.0;
  float windGust = 0.0;
  float temperature = 0.0;
  float humidity = 0.0;
  float batVoltage = 0.0;
};

SensorData dataToSend;  // Declare a global instance of SensorData

void setup() {
  Serial.begin(115200);
  SPI.begin(SCK, MISO, MOSI, SS);  // Initialize SPI for LoRa
  LoRa.setPins(SS, RST, DI0);  // Set LoRa pins

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);  // Halt if LoRa init fails
  }

  Serial.println("LoRa Initialized");
}

void loop() {
  String serialData = "";  // Store incoming data from Serial

  // Read incoming serial data (this could be from a sensor)
  while (Serial.available()) {
    char incomingByte = Serial.read();
    serialData += incomingByte;
  }

  // If there is data, parse it
  if (serialData.length() > 0) {
    parseData(serialData);  // Parse the received data
  }

  // Send the parsed data over LoRa (for testing purposes)
  sendLoRaPacket();

  delay(10000);  // Send data every 10 seconds (adjust as needed)
}

// ==== Parse Incoming Serial Data ====
void parseData(String data) {
  data.trim();  // Clean the data

  // Parse specific values from the incoming data
  if (data.indexOf("WindDir") != -1) dataToSend.windDir = getValue(data, "WindDir");
  if (data.indexOf("WindSpeed") != -1) dataToSend.windSpeed = getValue(data, "WindSpeed") * 1.94384;  // Convert to knots
  if (data.indexOf("WindGust") != -1) dataToSend.windGust = getValue(data, "WindGust") * 1.94384;  // Convert to knots
  if (data.indexOf("Temperature") != -1) dataToSend.temperature = getValue(data, "Temperature");
  if (data.indexOf("Humi") != -1) dataToSend.humidity = getValue(data, "Humi");
  if (data.indexOf("BatVoltage") != -1) dataToSend.batVoltage = getValue(data, "BatVoltage");

  // Print parsed values to Serial Monitor
  Serial.println("\n📊 Parsed Data:");
  Serial.printf("🌬 WindDir: %d°\n", dataToSend.windDir);
  Serial.printf("💨 WindSpeed: %.1f knots\n", dataToSend.windSpeed);
  Serial.printf("🌪 WindGust: %.1f knots\n", dataToSend.windGust);
  Serial.printf("🌡 Temp: %.1f C\n", dataToSend.temperature);
  Serial.printf("💧 Humi: %.1f %%\n", dataToSend.humidity);
  Serial.printf("🔋 BatVoltage: %.2f V\n", dataToSend.batVoltage);
}

// ==== Extract Values from Serial String ====
float getValue(String data, String key) {
  int keyPos = data.indexOf(key);  // Find position of key in the data
  if (keyPos != -1) {
    int startPos = data.indexOf("=", keyPos) + 1;  // Find where the value starts
    int endPos = data.indexOf("\n", startPos);  // Find where the value ends (newline character)
    String valueString = data.substring(startPos, endPos);  // Extract the value string
    valueString.trim();  // Remove any extra spaces
    return valueString.toFloat();  // Convert the value string to a float
  }
  return 0.0;  // If key is not found, return 0.0
}

void sendLoRaPacket() {
  // Prepare and send data over LoRa
  LoRa.beginPacket();
  LoRa.print("WindDir: " + String(dataToSend.windDir));
  LoRa.print(", WindSpeed: " + String(dataToSend.windSpeed));
  LoRa.print(" knots");
  LoRa.print(", WindGust: " + String(dataToSend.windGust));
  LoRa.print(" knots");
  LoRa.print(", Temp: " + String(dataToSend.temperature));
  LoRa.print(" C");
  LoRa.print(", Humi: " + String(dataToSend.humidity));
  LoRa.print(" %");
  LoRa.print(", BatVoltage: " + String(dataToSend.batVoltage));
  LoRa.endPacket();

  // Debug: Print the sent packet data
  Serial.println("Sent LoRa Packet: ");
  Serial.printf("WindDir: %d°, WindSpeed: %.1f knots, WindGust: %.1f knots, Temp: %.1f C, Humi: %.1f %%, BatVoltage: %.2f V\n",
                dataToSend.windDir, dataToSend.windSpeed, dataToSend.windGust, dataToSend.temperature, dataToSend.humidity, dataToSend.batVoltage);
}
