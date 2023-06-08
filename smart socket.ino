#include "BluetoothSerial.h"
#include <Preferences.h>
#include "UUID.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define BT_NAME "smart socket"
#define SSID_NAME_KEY "ssidName"
#define SSID_PASSWORD_KEY "ssidPassword"
#define MQTT_NAME_KEY "mqttPassword"
#define MQTT_PASSWORD_KEY "mqttPassword"
#define PRIVATE_KEY_KEY "private-key"
#define PUBLIC_KEY_KEY "public-key"
#define UUID_KEY "uuid"
#define PASSWORD_KEY "password"
#define PREFERENCES_SCOPE_KEY "rsa-keys"
#define TOTAL_ATTEMPTS 10

#define RELAY_1 26
#define RELAY_2 27

enum Mode {
  IDLEMode,
  BTMode,
  WiFiMode
} mode;

BluetoothSerial SerialBT;
WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;
UUID uuid;

String ssidName;
String ssidPassword;
String mqttLogin;
String mqttPassword;
const char* mqttServer = "mqtt.by";
const int mqttPort = 1883;
String mqttTopic;
String publicKey;
String privateKey;
String deviceUUID;

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  
  preferences.begin(PREFERENCES_SCOPE_KEY, false);  
  // preferences.putString(PASSWORD_KEY, "password"); // for init setting
  // preferences.putString(UUID_KEY, uuid.toCharArray());
  deviceUUID = preferences.getString(UUID_KEY);

  publicKey = preferences.getString(PUBLIC_KEY_KEY);
  privateKey = preferences.getString(PRIVATE_KEY_KEY);

  if (publicKey.equals("")) {
    mode = BTMode;
    SerialBT.begin(BT_NAME);
    
  } else {
    mode = WiFiMode;
    ssidName = preferences.getString(SSID_NAME_KEY);
    ssidPassword = preferences.getString(SSID_PASSWORD_KEY);
  }

  // preferences.clear();
  // preferences.putString("key", "somePrivateKey");
  // preferences.end();
  // put your setup code here, to run once:
}

void loop() {
  if (mode == BTMode) {
    btModeLogic();
  } else {
    wifiModeLogic();
  }
}

void btModeLogic() {
  if (SerialBT.available()) {
    Serial.println("BT mode");
    String data = SerialBT.readString();

    readIncomingData(data);

    if (!tryConnectToWifi()) {
      return;
    }

    if (!tryConnectToMqtt()) {
      return;
    }

    SerialBT.print(deviceUUID + "#" + "smart socket");
    SerialBT.print(deviceUUID + "#" + "smart socket");
  // SerialBT.print("A");
  }
}

void wifiModeLogic() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Reconnecting to MQTT server...");
    if (client.connect("ESP32Client", (char*) mqttLogin.c_str(), (char*) mqttPassword.c_str())) {
      Serial.println("Connected to MQTT server");
      client.subscribe((char*) mqttTopic.c_str());
    } else {
      Serial.print("Failed to connect to MQTT server, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void readIncomingData(String data) {
  int separators[4];
  getSeparatorIndexies(data, separators);
  ssidName = data.substring(0, separators[0]);
  ssidPassword = data.substring(separators[0] + 1, separators[1]);
  mqttLogin = data.substring(separators[1] + 1, separators[2]);
  mqttPassword = data.substring(separators[2] + 1, separators[3]);
  mqttTopic = data.substring(separators[3] + 1, data.length());
}

void getSeparatorIndexies(String data, int *emptyArr) {
  int tempIndex = 0;
  for (int i = 0; i < data.length(); i++) {
    if (data[i] == '#') {
      emptyArr[tempIndex++] = i;
    }
  }
}

bool tryConnectToWifi() {
  WiFi.begin((char*) ssidName.c_str(), (char*) ssidPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    attempts++;
    if (attempts > TOTAL_ATTEMPTS) {
      String messageError = "Can`t connect to Wi-Fi, try to use another credentials";
      Serial.println(messageError);
      SerialBT.print(messageError);
      return false;
    }
  }

  preferences.putString(SSID_NAME_KEY, ssidName);
  preferences.putString(SSID_PASSWORD_KEY, ssidPassword);

  Serial.println("Connected to WiFi");

  mode = WiFiMode;
  return true;
}

bool tryConnectToMqtt() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  int attempts = 0;
  while (!client.connected()) {
    Serial.println("Connecting to MQTT server...");
    if (client.connect("ESP32Client", (char*) mqttLogin.c_str(), (char*) mqttPassword.c_str())) {
      Serial.println("Connected to MQTT server");
    } else {
      if (attempts > TOTAL_ATTEMPTS) {
        String messageError = "Can`t connect to MQTT, try to use another credentials";
        Serial.println(messageError);
        SerialBT.print(messageError);
        return false;
      }
      Serial.print("Failed to connect to MQTT server, rc=");
      Serial.println(client.state());
      attempts++;
      delay(5000);
    }
  }

  client.subscribe((char*) mqttTopic.c_str());
  return true;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String command;
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }

  String commandUUID;
  String commandValue;
  for (int i = 0; i < command.length(); i++) {
    if (command[i] == '#') {
      commandUUID = command.substring(0, i);
      commandValue = command.substring(i + 1, command.length());
      break;
    }
  }

  if (commandUUID == deviceUUID) {
    digitalWrite(RELAY_1, commandValue.equals("off"));
    digitalWrite(RELAY_2, commandValue.equals("off"));    
  }
}
