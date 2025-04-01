#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define SCK     5
#define MISO    19
#define MOSI    27
#define SS      18
#define RST     23
#define DI0     26
#define BAND    868E6

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TEXT_SIZE 1
#define CURSOR_Y_SPACING 10

#define MQTT_SWITCH_PIN 4  // GPIO pin for the MQTT toggle switch
#define TX_PIN 1
#define RX_PIN 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqttServer = "152.xxxxxxxx";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";
const char* mqttTopic = "weather/data/WS80_Lora";

int windDir = 0;
float windSpeed = 0.0, windGust = 0.0, temperature = 0.0, humidity = 0.0;
#define WIND_SPEED_UNIT "knots"

String receivedPacket = "";

void setup() {
    Serial.begin(115200);
    pinMode(MQTT_SWITCH_PIN, INPUT_PULLUP);
    pinMode(TX_PIN, OUTPUT);
    pinMode(RX_PIN, INPUT_PULLUP);
    Wire.begin(21, 22);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed!");
        while (1);
    }
    
    display.clearDisplay();
    display.setTextSize(TEXT_SIZE);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("LoRa Receiver Init");
    display.display();

    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DI0);

    if (!LoRa.begin(BAND)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    Serial.println("LoRa Receiver Ready");

    WiFiManager wifiManager;
    wifiManager.autoConnect("LoRa_Weather_Receiver");
    Serial.println("WiFi connected");

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
}

void loop() {
    digitalWrite(TX_PIN, LOW);
    bool mqttEnabled = digitalRead(RX_PIN) == LOW;
    
    if (mqttEnabled && !mqttClient.connected()) {
        reconnectMQTT();
    }
    if (mqttEnabled) {
        mqttClient.loop();
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        receivedPacket = "";
        while (LoRa.available()) {
            receivedPacket += (char)LoRa.read();
        }
        
        Serial.println("Received: " + receivedPacket);
        parseData(receivedPacket);
        displayData(mqttEnabled);
        if (mqttEnabled) {
            sendDataToMQTT();
        }
    }
}

void parseData(String data) {
    int valuePos;
    if ((valuePos = data.indexOf("WindDir: ")) != -1)
        windDir = data.substring(valuePos + 9).toInt();
    if ((valuePos = data.indexOf("WindSpeed: ")) != -1)
        windSpeed = data.substring(valuePos + 11).toFloat();
    if ((valuePos = data.indexOf("WindGust: ")) != -1)
        windGust = data.substring(valuePos + 10).toFloat();
    if ((valuePos = data.indexOf("Temp: ")) != -1)
        temperature = data.substring(valuePos + 6).toFloat();
    if ((valuePos = data.indexOf("Humi: ")) != -1)
        humidity = data.substring(valuePos + 6).toFloat();
    
    if (String(WIND_SPEED_UNIT) == "knots") {
        windSpeed *= 1.94384;
        windGust *= 1.94384;
    }
}

void displayData(bool mqttEnabled) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("WindSpeed: %.1f %s\n", windSpeed, WIND_SPEED_UNIT);
    display.setCursor(0, 15);
    display.printf("WindGust: %.1f %s\n", windGust, WIND_SPEED_UNIT);
    display.setCursor(0, 30);
    display.printf("WindDir: %d\n", windDir);
    display.setCursor(0, 45);
    display.printf("Temp: %.1f C\n", temperature);
    display.setCursor(0, 55);
    display.printf("MQTT: %s\n", mqttEnabled ? "ON" : "OFF");
    display.display();
}

// check wind m_s or knots = *1.94384
void sendDataToMQTT() {
    String payload = String("{\"model\": \"WS80_LoraReceiver\", ") +
                     String("\"id\": \"l-receiver-kwind-1\", ") +
                     String("\"wind_dir_deg\": ") + String(windDir) + ", " +
                     String("\"wind_avg_m_s\": ") + String(windSpeed) + ", " +
                     String("\"wind_max_m_s\": ") + String(windGust) + ", " +
                     String("\"temperature_C\": ") + String(temperature) + ", " +
                     String("\"Humi\": ") + String(humidity) + "}";

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
}
