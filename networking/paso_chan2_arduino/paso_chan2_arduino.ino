/*
 * ESP32 Client
 * Freenove ESP32 WROOM with 0.96" OLED Display
 * 
 * Hardware Connections (I2C):
 * - OLED SDA -> GPIO 21
 * - OLED SCL -> GPIO 22
 * - OLED VCC -> 3.3V
 * - OLED GND -> GND
 * 
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssid = "WIFIUSERNAME";           
const char* password = "WIFIPASSWORD";  
const char* serverIP = "ServerIP";        
const int serverPort = 8888;

// Device ID
const char* deviceName = "Device2";
// ==========================================

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi and Server
WiFiClient client;
bool connected = false;

// Message handling
String lastMessage = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 10000; // Send test message every 10 seconds
int messageCount = 0;

// Button for testing (just the boot button on the board)
#define BUTTON_PIN 0
bool lastButtonState = HIGH;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n=== Tamagotchi ESP32 Client ===");
  Serial.print("Device: ");
  Serial.println(deviceName);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);// loop forever
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Tamagotchi");
  display.println(deviceName);
  display.println();
  display.println("Starting...");
  display.display();
  delay(2000);
  
  // Connect to WiFi
  connectWiFi();
  
  // Connect to server
  connectToServer();
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectWiFi();
  }
  
  // Check server connection
  if (!client.connected()) {
    connected = false;
    Serial.println("Server disconnected. Reconnecting...");
    connectToServer();
  }
  
  // Read messages from server
  if (client.available()) {
    String message = client.readStringUntil('\n');
    message.trim();
    
    if (message.length() > 0) {
      Serial.print("Received: ");
      Serial.println(message);
      
      lastMessage = message;
      displayMessage(message);
    }
  }
  
  // Check button press to send message
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // Debounce
    sendMessage("Button pressed!");
    lastButtonState = LOW;
  } else if (buttonState == HIGH) {
    lastButtonState = HIGH;
  }
  
  // Send periodic test messages
  if (millis() - lastSendTime > SEND_INTERVAL) {
    messageCount++;
    String msg = String(deviceName) + " msg #" + String(messageCount);
    sendMessage(msg);
    lastSendTime = millis();
  }
  
  delay(100);
}

void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to");
  display.println(ssid);
  display.display();
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  } else {
    Serial.println("\nWiFi connection failed!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi FAILED");
    display.println("Check settings");
    display.display();
    delay(5000);
  }
}

void connectToServer() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to");
  display.println("server...");
  display.print(serverIP);
  display.print(":");
  display.println(serverPort);
  display.display();
  
  Serial.print("Connecting to server: ");
  Serial.print(serverIP);
  Serial.print(":");
  Serial.println(serverPort);
  
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Connected to server!");
    connected = true;
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connected!");
    display.println();
    display.println("Waiting for");
    display.println("messages...");
    display.display();
    
    delay(2000);
  } else {
    Serial.println("Connection to server failed!");
    connected = false;
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Server FAILED");
    display.println("Retrying...");
    display.display();
    
    delay(5000);
  }
}

void sendMessage(String message) {
  if (connected && client.connected()) {
    client.println(message);
    Serial.print("Sent: ");
    Serial.println(message);
    
    // Show on display briefly
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SENT:");
    display.println();
    display.println(message);
    display.display();
    delay(1000);
  } else {
    Serial.println("Cannot send - not connected");
  }
}

void displayMessage(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("RECEIVED:");
  display.println();
  
  // Word wrap for long messages
  display.setTextSize(1);
  int cursorY = 16;
  String word = "";
  int lineWidth = 0;
  
  for (int i = 0; i < message.length(); i++) {
    char c = message[i];
    
    if (c == ' ' || i == message.length() - 1) {
      if (i == message.length() - 1 && c != ' ') {
        word += c;
      }
      
      int wordWidth = word.length() * 6;
      
      if (lineWidth + wordWidth > SCREEN_WIDTH) {
        cursorY += 10;
        lineWidth = 0;
      }
      
      display.setCursor(lineWidth, cursorY);
      display.print(word);
      
      if (c == ' ') {
        display.print(" ");
        lineWidth += wordWidth + 6;
      } else {
        lineWidth += wordWidth;
      }
      
      word = "";
    } else {
      word += c;
    }
  }
  
  display.display();
}
