#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== LoRa Configuration ======
#define SCK     5    // LoRa SCK Pin
#define MISO    19   // LoRa MISO Pin
#define MOSI    27   // LoRa MOSI Pin
#define SS      18   // LoRa SS Pin
#define RST     23   // LoRa RST Pin
#define DI0     26   // LoRa DIO0 Pin
#define BAND    868E6 // LoRa frequency (868 MHz)

// ====== OLED Display Configuration ======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TEXT_SIZE 1
#define CURSOR_Y_SPACING 25

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ====== Global Variables ======
struct SensorData {
  int windDir = 0;
  float windSpeed = 0.0;  // m/s
  float windGust = 0.0;
  float temperature = 0.0;
  float humidity = 0.0;
  float batVoltage = 0.0;
};

SensorData dataToSend;
bool displayEnabled = true;  // Set to false to disable OLED display

unsigned long lastSendTime = 0; // Time tracking for LoRa sending
unsigned long serialReadInterval = 5000; // 5 seconds interval for sending LoRa data
unsigned long lastSerialReadTime = 0;  // Timer for reading data
String serialData = "";  // Buffer for incoming data
bool sendDataFlag = false;  // Flag to trigger data send every 5 seconds

// ====== Setup ======
void setup() {
  Serial.begin(115200);

  // Initialize SPI and LoRa
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("🔵 LoRa KWind");
  display.display();

  // Initialize LoRa
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("✅ LoRa Initialized");
}

// ====== Loop ======
void loop() {
  // Read and buffer serial data if available
  if (Serial.available()) {
    char incomingByte = Serial.read();
    serialData += incomingByte;

    // If a complete packet (e.g., ending with newline) is received, parse it
    if (incomingByte == '\n') {
      serialData.trim();  // Clean up the received string
      parseData(serialData);
      serialData = "";  // Clear the buffer after processing
    }
  }

  // Check if it's time to send data over LoRa
  if (millis() - lastSendTime >= serialReadInterval) {
    sendDataFlag = true;  // Set flag to send data over LoRa
    lastSendTime = millis();  // Reset send timer
  }

  // If sendDataFlag is set, send data over LoRa and reset the flag
  if (sendDataFlag) {
    sendLoRaPacket();
    sendDataFlag = false;  // Reset the flag after sending data
  }
}

// ====== Parse Incoming Serial Data ======
void parseData(String data) {
  data.trim();  // Clean incoming string

  if (data.indexOf("WindDir") != -1) dataToSend.windDir = getValue(data, "WindDir");
  if (data.indexOf("WindSpeed") != -1) dataToSend.windSpeed = getValue(data, "WindSpeed");  // m/s 
  if (data.indexOf("WindGust") != -1) dataToSend.windGust = getValue(data, "WindGust");
  if (data.indexOf("Temperature") != -1) dataToSend.temperature = getValue(data, "Temperature");
  if (data.indexOf("Humi") != -1) dataToSend.humidity = getValue(data, "Humi");
  if (data.indexOf("BatVoltage") != -1) dataToSend.batVoltage = getValue(data, "BatVoltage");
  // Display on OLED if enabled
  if (displayEnabled) {
    displayData();
  }
}

// ====== Extract Value from Serial String ======
float getValue(String data, String key) {
  int keyPos = data.indexOf(key);
  if (keyPos != -1) {
    int startPos = data.indexOf("=", keyPos) + 1;
    int endPos = data.indexOf("\n", startPos);
    if (endPos == -1) endPos = data.length(); // If no newline, take until end
    String valueString = data.substring(startPos, endPos);
    valueString.trim();
    return valueString.toFloat();
  }
  return 0.0;
}

// ====== Send Packet over LoRa ======
void sendLoRaPacket() {
  LoRa.beginPacket();
  LoRa.print("WindDir: " + String(dataToSend.windDir));
  LoRa.print(", WindSpeed: " + String(dataToSend.windSpeed) + " m/s");  // m/s
  LoRa.print(", WindGust: " + String(dataToSend.windGust) + " m/s");  // m/s
  LoRa.print(", Temp: " + String(dataToSend.temperature) + " C");
  LoRa.print(", Humi: " + String(dataToSend.humidity) + " %");
  LoRa.print(", BatVoltage: " + String(dataToSend.batVoltage) + " V");
  LoRa.endPacket();
// Debug Print
  Serial.println("\n📊 Parsed Data:");
  Serial.printf("🌬 WindDir: %d°\n", dataToSend.windDir);
  Serial.printf("💨 WindSpeed: %.1f m/s\n", dataToSend.windSpeed);  // Confirm in m/s
  Serial.printf("🌪 WindGust: %.1f m/s\n", dataToSend.windGust);  // Confirm in m/s
  Serial.printf("🌡 Temp: %.1f °C\n", dataToSend.temperature);
  Serial.printf("💧 Humi: %.1f %%\n", dataToSend.humidity);
  Serial.printf("🔋 BatVoltage: %.2f V\n", dataToSend.batVoltage);
  Serial.println("📡 Sent LoRa Packet:");
  Serial.printf("WindDir: %d°, WindSpeed: %.1f m/s, WindGust: %.1f m/s, Temp: %.1f°C, Humi: %.1f%%, BatVoltage: %.2fV\n",
                dataToSend.windDir, dataToSend.windSpeed, dataToSend.windGust, dataToSend.temperature, dataToSend.humidity, dataToSend.batVoltage);
}

// ====== Display Data on OLED ======
void displayData() {
  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
  display.setCursor(0, 0);
  display.printf("Dir: %d deg\n", dataToSend.windDir);
  display.setCursor(0, 15);
  display.printf("Spd: %.1f m/s\n", dataToSend.windSpeed);  // m/s
  display.setCursor(0, 30);
  display.printf("Gust: %.1f m/s\n", dataToSend.windGust);  // m/s
  display.setCursor(0, 45);
  display.printf("Tmp: %.1f C\n", dataToSend.temperature);
  display.setCursor(0, 57);
  display.printf("Humi: %.0f %%\n", dataToSend.humidity);
  display.setCursor(65, 57);
  display.printf("Bat: %.1f V\n", dataToSend.batVoltage);

  display.display();
}
