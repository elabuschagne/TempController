#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Define the number of strings to store
#define NUM_STRINGS 10

// Define the length of each string
#define STRING_LENGTH 20

// Define the starting address for the string data in EEPROM
#define STRING_DATA_START_ADDRESS 0

// Define the size of each string block in EEPROM
#define STRING_BLOCK_SIZE (STRING_LENGTH + 1) // Add 1 for null terminator

// Define the starting address for the integer data in EEPROM
#define INT_DATA_START_ADDRESS 200

// Define the size of each integer block in EEPROM
#define INT_BLOCK_SIZE 2

// Define the indices for the stored strings and integers in EEPROM
#define SSID_INDEX 0
#define PASSWORD_INDEX 1
#define MQTT_SERVER_INDEX 2
#define MQTT_PORT_INDEX 3
#define MQTT_CLIENT_ID_INDEX 4
#define MQTT_USER_INDEX 5
#define MQTT_PASSWORD_INDEX 6

// Define default values for SSID, password, MQTT server, port, client ID, user, and password
#define DEFAULT_SSID "LabuHome Basement"
#define DEFAULT_PASSWORD "Labs@Canada!"
#define DEFAULT_MQTT_SERVER "192.168.2.119"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "etienne"
#define DEFAULT_MQTT_PASSWORD "Foxbat1024!"

// set up the wifi and mqtt clients
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// publisher methods
void publish(String message) {
  String mqttClientId = getHardwareId();
  String sendTopic = "devices/" + mqttClientId + "/send";
  mqttClient.publish(sendTopic.c_str(), message.c_str());
}

void publishJson(const JsonObject& json){
  // Serialize the JSON object to a string
  String jsonString;
  serializeJson(json, jsonString);
  publish(jsonString);
}

// handler methods
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Parse the message payload as JSON
  DynamicJsonDocument doc(length);
  deserializeJson(doc, payload, length);
  
  // Extract the "type" key from the JSON object
  const char* type = doc["type"];

  // Call the appropriate handler method based on the message type
  if (strcmp(type, "command") == 0) {
    handleCommand(doc.as<JsonObject>());
  } else {
    publishErrorJson("Unknown type", "Valid types are [command]");
  }
}

void publishErrorJson(String error, String detail) {
    Serial.println(error);
    Serial.println(detail);

    DynamicJsonDocument jsonDoc(1024);
    jsonDoc["error"] = error;
    jsonDoc["detail"] = detail;
    publishJson(jsonDoc.as<JsonObject>());  
}

// Handler methods for each message type
void handleCommand(const JsonObject& json) {
  const char* command = json["command"];
  if (strcmp(command, "reboot") == 0) {
    reboot();
  } else if (strcmp(command, "clear") == 0) {
    clearEEPROM();
  } else {
    publishErrorJson("Unknown command", "Valud commands are [reboot, clear]");
  }
}


// EEPROM methods
void clearEEPROM() {
  Serial.println("Clearing EEPROM...");

  for (int i = 0; i < NUM_STRINGS; i++) {
    // Calculate the starting address for this string in EEPROM
    int address = STRING_DATA_START_ADDRESS + (i * STRING_BLOCK_SIZE);

    // Write null characters to the string data in EEPROM
    for (int j = 0; j < STRING_LENGTH; j++) {
      EEPROM.write(address + j, '\0');
    }
  }

  // Commit the changes to EEPROM
  EEPROM.commit();
  Serial.println("EEPROM cleared.");
}

void reboot() {
      Serial.println("Rebooting...");
      delay(1000);
      ESP.restart();  
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();

    Serial.println("");
    Serial.println("Got command: " + input);

    if (input == "clear") {
      clearEEPROM();
    } else if (input == "reboot") {
      reboot();
    } else {
      Serial.println("Unknown command, valid commands are: [reboot, clear]");
    }
  }
}

void storeStringToEEPROM(int index, const String& str) {
  // Calculate the starting address for this string in EEPROM
  int address = STRING_DATA_START_ADDRESS + (index * STRING_BLOCK_SIZE);

  // Write the string data to EEPROM
  for (int i = 0; i < STRING_LENGTH; i++) {
    EEPROM.write(address + i, str.charAt(i));
  }

  // Add a null terminator at the end of the string
  EEPROM.write(address + STRING_LENGTH, '\0');

  // Commit the changes to EEPROM
  EEPROM.commit();
}

String getStringFromEEPROM(int index) {
  // Calculate the starting address for this string in EEPROM
  int address = STRING_DATA_START_ADDRESS + (index * STRING_BLOCK_SIZE);

  // Read the string data from EEPROM into a temporary buffer
  char buffer[STRING_LENGTH + 1];  // Add 1 for the null terminator
  for (int i = 0; i < STRING_LENGTH; i++) {
    buffer[i] = EEPROM.read(address + i);
  }
  buffer[STRING_LENGTH] = '\0';  // Add null terminator

  // Convert the buffer to a String and return it
  return String(buffer);
}

void storeIntToEEPROM(int index, int value) {
  // Calculate the starting address for this integer in EEPROM
  int address = INT_DATA_START_ADDRESS + (index * INT_BLOCK_SIZE);

  // Split the integer value into MSB and LSB
  byte msb = (value >> 8) & 0xFF;   // Shift the value 8 bits to the right and take the 8 most significant bits
  byte lsb = value & 0xFF;          // Take the 8 least significant bits

  // Write the MSB and LSB to EEPROM
  EEPROM.write(address, msb);
  EEPROM.write(address + 1, lsb);

  // Commit the changes to EEPROM
  EEPROM.commit();
}

int getIntFromEEPROM(int index) {
  // Calculate the starting address for this integer in EEPROM
  int address = INT_DATA_START_ADDRESS + (index * INT_BLOCK_SIZE);

  // Read the MSB and LSB from EEPROM
  byte msb = EEPROM.read(address);
  byte lsb = EEPROM.read(address + 1);

  // Combine the MSB and LSB into an integer value
  int value = (msb << 8) | lsb;

  // Return the integer value
  return value;
}

String getHardwareId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String hardwareId = "";
  for (int i = 0; i < 6; i++) {
    hardwareId += String(mac[i], HEX);
    if (i < 5) {
      hardwareId += "-";
    }
  }
  return hardwareId;
}

// setup methods
void setupWifi() {
  // Get the SSID and password from EEPROM, or use default values if not set
  String ssid = getStringFromEEPROM(SSID_INDEX);
  if (ssid.length() == 0) {
    ssid = DEFAULT_SSID;
  }
  String password = getStringFromEEPROM(PASSWORD_INDEX);
  if (password.length() == 0) {
    password = DEFAULT_PASSWORD;
  }

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi (SSID: ");
  Serial.print(ssid);
  Serial.print(", Password: ");
  if (password.length() > 2) {
    Serial.print(password.charAt(0));
    Serial.print("****");
    Serial.print(password.charAt(password.length()-1));
  } else {
    Serial.print(password);
  }
  Serial.print(")");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    checkSerialInput();
  }
  Serial.println(" connected");
}

void setupMQTT() {
  // Get the MQTT server, port, client ID, user, and password from EEPROM, or use default values if not set
  String mqttServer = getStringFromEEPROM(MQTT_SERVER_INDEX);
  if (mqttServer.length() == 0) {
    mqttServer = DEFAULT_MQTT_SERVER;
  }
  int mqttPort = getIntFromEEPROM(MQTT_PORT_INDEX);
  if (mqttPort == 0) {
    mqttPort = DEFAULT_MQTT_PORT;
  }
  String mqttClientId = getHardwareId();

  String mqttUser = getStringFromEEPROM(MQTT_USER_INDEX);
  if (mqttUser.length() == 0) {
    mqttUser = DEFAULT_MQTT_USER;
  }
  String mqttPassword = getStringFromEEPROM(MQTT_PASSWORD_INDEX);
  if (mqttPassword.length() == 0) {
    mqttPassword = DEFAULT_MQTT_PASSWORD;
  }

  // Connect to MQTT broker
  Serial.print("Connecting to MQTT broker (Server: ");
  Serial.print(mqttServer);
  Serial.print(", Port: ");
  Serial.print(mqttPort);
  Serial.print(", Client ID: ");
  Serial.print(mqttClientId);
  Serial.print(", User: ");
  Serial.print(mqttUser);
  Serial.print(", Password: ");
  if (mqttPassword.length() > 2) {
    Serial.print(mqttPassword.charAt(0));
    Serial.print("****");
    Serial.print(mqttPassword.charAt(mqttPassword.length()-1));
  } else {
    Serial.print(mqttPassword);
  }
  Serial.print(")");

  mqttClient.setServer(mqttServer.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    checkSerialInput();
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPassword.c_str())) {
      Serial.println("connected");
      String receiveTopic = "devices/" + mqttClientId + "/receive";
      mqttClient.subscribe(receiveTopic.c_str());
      Serial.print("Listening on topic '" + receiveTopic + "'");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512); // Initialize EEPROM with the maximum size you plan to use

  setupWifi();
  setupMQTT();

  publish("Started up.");
}

void loop() {
  checkSerialInput();

  mqttClient.loop();

  // Store some sample data to EEPROM
  // storeStringToEEPROM(0, "Hello, world!");
  // storeStringToEEPROM(1, "This is a test.");
  // storeStringToEEPROM(2, "1234567890");
  // storeStringToEEPROM(3, "Some other data");

  // Retrieve and print the data from EEPROM
  // for (int i = 0; i < NUM_STRINGS; i++) {
  //   char data[STRING_LENGTH + 1];
  //   getStringFromEEPROM(i, data);
  //   Serial.print("String ");
  //   Serial.print(i);
  //   Serial.print(": ");
  //   Serial.println(data);
  // }

  // Wait for 10 seconds before repeating the loop
  delay(10000);
  Serial.println(getHardwareId());
}

