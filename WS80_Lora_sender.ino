#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include "SSD1306.h"

// **Flag to enable/disable OLED display**
#define USE_DISPLAY true  

// LoRa Configuration
#define SCK     5    
#define MISO    19   
#define MOSI    27   
#define SS      18   
#define RST     23   
#define DI0     26   
#define BAND    868E6

// Wind data variables
int windDir = 0;
float windSpeed = 0.0, windGust = 0.0, temperature = 0.0, humidity = 0.0;

// Define wind speed unit (choose between "ms" for meters per second or "knots" for knots)
#define WIND_SPEED_UNIT "knots"  // Options: "ms" or "knots"

#if USE_DISPLAY
SSD1306 display(0x3c, 4, 15);  // OLED display setup
#endif

String serialBuffer = "";  // Buffer for incoming serial data
unsigned long lastUpdateTime = 0;  

void setup() {
  Serial.begin(115200);  
  Serial2.begin(115200, SERIAL_8N1, 1, 3);  

  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);
  
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);  
  }
  
  Serial.println("LoRa Initialized");

#if USE_DISPLAY
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    
  delay(50); 
  digitalWrite(16, HIGH);

  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
  display.display();
  delay(1500);
#endif
}

void loop() {
  // Read incoming serial data from Serial2
  while (Serial2.available()) {
    char incomingByte = Serial2.read();
    serialBuffer += incomingByte;
  }

  // Debugging: Print raw incoming data to Serial Monitor
  if (serialBuffer.length() > 0) {
    Serial.print("Raw incoming data: ");
    Serial.println(serialBuffer);  // Print the raw data received
  }

  // Check if data contains "SHT30" identifier for valid sensor data
  if (serialBuffer.indexOf("SHT30") != -1) {
    Serial.println("Valid data received: " + serialBuffer);  
    parseData(serialBuffer);  // Parse the received data
    serialBuffer = "";  // Clear the buffer after parsing

#if USE_DISPLAY
    displayData();  // Display the parsed data on OLED
#endif
  }

  // Send packet every 10 seconds (regardless of incoming data)
  if (millis() - lastUpdateTime >= 10000) {
    sendLoRaPacket();
    lastUpdateTime = millis();  // Update the time of the last packet sent
  }
}

void parseData(String data) {
  // Clean the data by removing extra spaces and unnecessary characters
  data.trim();  // Removes leading/trailing whitespaces

  int valuePos;

  // Extracting values for wind direction, speed, gust, temperature, and humidity
  if (data.indexOf("WindDir") != -1) {
    windDir = getValue(data, "WindDir");
  }

  if (data.indexOf("WindSpeed") != -1) {
    windSpeed = getValue(data, "WindSpeed");
    // Convert wind speed to knots if WIND_SPEED_UNIT is set to "knots"
    if (String(WIND_SPEED_UNIT) == "knots") {
      windSpeed = windSpeed * 1.94384;  // Convert m/s to knots
    }
  }

  if (data.indexOf("WindGust") != -1) {
    windGust = getValue(data, "WindGust");
    // Convert wind gust to knots if WIND_SPEED_UNIT is set to "knots"
    if (String(WIND_SPEED_UNIT) == "knots") {
      windGust = windGust * 1.94384;  // Convert m/s to knots
    }
  }

  if (data.indexOf("Temperature") != -1) {
    temperature = getValue(data, "Temperature");
  }

  if (data.indexOf("Humi") != -1) {
    humidity = getValue(data, "Humi");
  }

  // Debugging parsed values - print to Serial Monitor
  Serial.println("Parsed Data:");
  Serial.printf("WindDir: %d\n", windDir);
  Serial.printf("WindSpeed: %.1f %s\n", windSpeed, WIND_SPEED_UNIT);  // Print the unit used
  Serial.printf("WindGust: %.1f %s\n", windGust, WIND_SPEED_UNIT);  // Print the unit used
  Serial.printf("Temp: %.1f C\n", temperature);
  Serial.printf("Humi: %.1f %%\n", humidity);
}

// Helper function to get the value from a string based on the key
float getValue(String data, String key) {
  int keyPos = data.indexOf(key);
  if (keyPos != -1) {
    int startPos = data.indexOf("=", keyPos) + 1;  // Find where the value starts
    int endPos = data.indexOf("\n", startPos);  // Find where the value ends
    String valueString = data.substring(startPos, endPos);
    
    // Remove any spaces and convert to the correct data type (float or int)
    valueString.trim();
    if (key == "Humi") {
      return valueString.toFloat();  // Humidity is a float
    } else {
      return valueString.toFloat();  // Other values are floats as well
    }
  }
  return 0.0;  // If the key was not found, return a default value
}

#if USE_DISPLAY
void displayData() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "WindSpeed: " + String(windSpeed) + " " + String(WIND_SPEED_UNIT));
  display.drawString(0, 15, "WindGust: " + String(windGust) + " " + String(WIND_SPEED_UNIT));
  display.drawString(0, 30, "WindDir: " + String(windDir));
  display.drawString(0, 45, "Temp: " + String(temperature) + " C");
  //display.drawString(0, 60, "Humi: " + String(humidity) + " %");

  display.display();
}
#endif

void sendLoRaPacket() {
  // Prepare and send data via LoRa every 10 seconds
  LoRa.beginPacket();
  LoRa.print("WindDir: " + String(windDir));
  LoRa.print(", WindSpeed: " + String(windSpeed));
  LoRa.print(" " + String(WIND_SPEED_UNIT));  // Include the unit (m/s or knots)
  LoRa.print(", WindGust: " + String(windGust));
  LoRa.print(" " + String(WIND_SPEED_UNIT));  // Include the unit (m/s or knots)
  LoRa.print(", Temp: " + String(temperature));
  LoRa.print(", Humi: " + String(humidity));
  LoRa.endPacket();

  Serial.println("Sent packet: " + String(windDir) + ", " + String(windSpeed) + " " + String(WIND_SPEED_UNIT) +
                  ", " + String(windGust) + " " + String(WIND_SPEED_UNIT) + ", " + String(temperature) + ", " + String(humidity));  // Debugging sent data
}
