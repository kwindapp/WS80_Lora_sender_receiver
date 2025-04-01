#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>      // WiFiManager library
#include <PubSubClient.h>     // MQTT library

// LoRa Module Pin Configuration
#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     23   // GPIO23 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ (Interrupt Request)
#define BAND    868E6 // LoRa Frequency (Make sure sender & receiver use the same)

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TEXT_SIZE 1
#define CURSOR_Y_SPACING 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Wind data variables
int windDir = 0;
float windSpeed = 0.0, windGust = 0.0, temperature = 0.0, humidity = 0.0;

// Wind speed unit setting ("ms" for meters per second or "knots" for knots)
#define WIND_SPEED_UNIT "knots"  // Options: "ms" or "knots"

// Received packet storage
String receivedPacket = "";

// WiFi and MQTT setup
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqttServer = "152.xxxxxxx";  // Correct IP address or hostname for MQTT broker
const int mqttPort = 1883;
const char* mqttUser = ""; // MQTT username (optional)
const char* mqttPassword = ""; // MQTT password (optional)
const char* mqttTopic = "weather/data/WS80_Lora";  // MQTT topic to publish data

void setup() {
  Serial.begin(115200);  // Debugging via USB Serial
  Wire.begin(21, 22);  // TTGO LoRa V1 I2C Pins (GPIO21 and GPIO22)

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  
    Serial.println("SSD1306 allocation failed! Trying 0x3D...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("SSD1306 not found. Check wiring!");
      while (1);  // Halt execution
    }
  }

  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("LoRa Receiver Init");
  display.display();

  // Initialize LoRa Module
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1); // Halt execution if LoRa module fails to initialize
  }

  Serial.println("LoRa Receiver Ready");

  // Initialize WiFi using WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("LoRa_Weather_Receiver");
  Serial.println("WiFi connected");

  // Setup MQTT client
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  // Ensure MQTT connection is alive
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    receivedPacket = "";
    
    while (LoRa.available()) {
      receivedPacket += (char)LoRa.read();
    }

    Serial.println("Received: " + receivedPacket);
    parseData(receivedPacket);
    displayData();
    sendDataToMQTT();
  }
}

void parseData(String data) {
  Serial.println("Parsing data...");

  int valuePos;

  // Extract Wind Direction (WindDir)
  if ((valuePos = data.indexOf("WindDir: ")) != -1)
    windDir = data.substring(valuePos + 9).toInt();
  
  // Extract Wind Speed (WindSpeed)
  if ((valuePos = data.indexOf("WindSpeed: ")) != -1)
    windSpeed = data.substring(valuePos + 11).toFloat();
  
  // Extract Wind Gust (WindGust)
  if ((valuePos = data.indexOf("WindGust: ")) != -1)
    windGust = data.substring(valuePos + 10).toFloat();
  
  // Extract Temperature (Temperature)
  if ((valuePos = data.indexOf("Temp: ")) != -1)
    temperature = data.substring(valuePos + 6).toFloat();
  
  // Extract Humidity (Humi)
  if ((valuePos = data.indexOf("Humi: ")) != -1)
    humidity = data.substring(valuePos + 6).toFloat();

  // If the wind speed and gust are in meters per second, convert to knots
  if (String(WIND_SPEED_UNIT) == "knots") {
    windSpeed *= 1.94384;  // Convert from m/s to knots
    windGust *= 1.94384;   // Convert from m/s to knots
  }

  // Debug parsed values
  Serial.printf("WindDir: %d, WindSpeed: %.1f %s, WindGust: %.1f %s, Temp: %.1f, Humi: %.1f\n", 
                windDir, windSpeed, WIND_SPEED_UNIT, windGust, WIND_SPEED_UNIT, temperature, humidity);
}

void displayData() {
  // Clear the previous content on the display
  display.clearDisplay();
  display.setTextSize(1);
  
  // Display Wind Speed
  display.setCursor(0, 0);
  display.printf("WindSpeed: %.1f %s\n", windSpeed, WIND_SPEED_UNIT);
  
  // Display Wind Gust
  display.setCursor(0, 15);
  display.printf("WindGust: %.1f %s\n", windGust, WIND_SPEED_UNIT);

  // Display Wind Direction
  display.setCursor(0, 30);
  display.printf("WindDir: %d\n", windDir);
  
  // Display Temperature
  display.setCursor(0, 45);
  display.printf("Temp: %.1f C\n", temperature);
  
  // Update the OLED with the new data
  display.display();
}

void sendDataToMQTT() {
  // Create a JSON-like string to send to MQTT broker
  String payload = String("{\"model\": \"WS80_LoraReceiver\", ") +
                 String("\"id\": \"l-receiver-kwind-1\", ") +
                 String("\"WindDir\": ") + String(windDir) + ", " +
                 String("\"WindSpeed\": ") + String(windSpeed) + ", " +
                 String("\"WindGust\": ") + String(windGust) + ", " +
                 String("\"Temp\": ") + String(temperature) + ", " +
                 String("\"Humi\": ") + String(humidity) + "}";

  // Publish the data to the MQTT topic
  if (mqttClient.publish(mqttTopic, payload.c_str())) {
    Serial.println("Data sent to MQTT successfully.");
  } else {
    Serial.println("Failed to send data to MQTT.");
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("LoRaReceiverClient", mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages (if needed)
}
