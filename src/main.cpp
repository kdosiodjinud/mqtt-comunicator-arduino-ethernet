#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <AsyncDelay.h>
// #include <ESP8266WiFi.h>

 //############ CONFIGURATION ################
 // wifi
const char* ssid = "Bifrost";
const char* password =  "CbSsPp42";
// ethernet 
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 199);
IPAddress server(192, 168, 1, 1);
 // mqtt server
const char* mqttServer = "192.168.1.100";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "admin";
// mqtt topics
char *mqttSubscribeTopics[] = { "home/workroom/lights/mainConfirm" };
//#############################################

int DIGITAL_PIN_3 = 3;
int DIGITAL_PIN_4 = 4;

/* 
struct settingsOutput {
  char* subscribedTopic;
  int pin;
  bool defaultLogic;
  bool valueForPing;
} settingsOutput[9] = {
  { "dum/pracovna/svetla/hlavni", D0, HIGH, LOW },
  { "dum/technicka/svetla/hlavni", D1, HIGH, LOW },
  { "dum/predsin/svetla/hlavni", D2, HIGH, LOW },
  { "dum/dolnichodba/svetla/hlavni", D3, HIGH, LOW },
  { "dum/spiz/svetla/hlavni", D4, HIGH, LOW },
  { "dum/dolnikoupelna/svetla/hlavni", D5, HIGH, LOW },
  { "dum/kuchyn/svetla/hlavni", D6, HIGH, LOW },
  { "dum/jidelna/svetla/hlavni", D7, HIGH, LOW },
  { "dum/obyvak/svetla/hlavni", D8, HIGH, LOW }
};
 */
/*
struct settingsInput {
  char* publishToTopic;
  int pin;
} settingsInput[9] = {
  { "dum/pracovna/svetla/hlavniPotvrzeni", D0 },
  { "dum/technicka/svetla/hlavniPotvrzeni", D1 },
  { "dum/predsin/svetla/hlavniPotvrzeni", D2 },
  { "dum/dolnichodba/svetla/hlavniPotvrzeni", D3 },
  { "dum/spiz/svetla/hlavniPotvrzeni", D4 },
  { "dum/dolnikoupelna/svetla/hlavniPotvrzeni", D5 },
  { "dum/kuchyn/svetla/hlavniPotvrzeni", D6 },
  { "dum/jidelna/svetla/hlavniPotvrzeni", D7 },
  { "dum/obyvak/svetla/hlavniPotvrzeni", D8 }
};
*/

// read topic and set digital pin 
struct settingsOutput {
  char* subscribedTopic;
  int pin;
  bool defaultLogic;
  bool valueForPing;
} settingsOutput[1] = {
  { "dum/pracovna/svetla/hlavni", DIGITAL_PIN_3, HIGH, LOW }
};


// read real state and publish it to confirm topic
struct settingsInput {
  char* publishToTopic;
  int pin;
} settingsInput[1] = {
  { "dum/pracovna/svetla/hlavniPotvrzeni", DIGITAL_PIN_4 }
};
int settingsInputPrevious[99];


// WiFiClient espClient;
EthernetClient ethClient;
PubSubClient client(ethClient);
AsyncDelay delay_1s;

// main methods definitions
void connectWifi();
void connectEthernet();
void connectMqtt();
void restartAllConnections();
void callback(char* topic, byte* payload, unsigned int length);
void setPortViaMqttTopicStatus(int port, char *topic, byte *payload, unsigned int length, bool onlyPing, bool valueForPing);
void publishPortStatusToMqtt(char* topic, int port);

// master setup method
void setup() {
  delay(5000);
  delay_1s.start(1000, AsyncDelay::MILLIS);
  Serial.begin(115200);

  // prepare output pins
  Serial.println("Setting pin mode for outputs.");
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    pinMode(settingsOutput[i].pin, OUTPUT);
    digitalWrite(settingsOutput[i].pin, settingsOutput[i].defaultLogic);
  }

  // prepare input pins
  Serial.println("Setting pin mode for inputs.");
  for (int i =0; i < sizeof(settingsInput) / sizeof(settingsInput[0]); i++) {
    pinMode(settingsInput[i].pin, INPUT);
  }

  // connectWifi();
  connectEthernet();
  connectMqtt();
}

void loop() {
  client.loop();
    for (int i =0; i < sizeof(settingsInput) / sizeof(settingsInput[0]); i++) {
      if (digitalRead(settingsInput[i].pin) != settingsInputPrevious[i]) {
        publishPortStatusToMqtt(settingsInput[i].publishToTopic, settingsInput[i].pin);
        settingsInputPrevious[i] = digitalRead(settingsInput[i].pin);
      }
    }
}

// MQTT subscribe
void callback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for ( unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // ##### SUBSCRIBE TOPICS AND MAKE REACTION HERE #######
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    if (strcmp(settingsOutput[i].subscribedTopic, topic) == 0) {
      setPortViaMqttTopicStatus(settingsOutput[i].pin, settingsOutput[i].subscribedTopic, payload, length, true, settingsOutput[i].valueForPing);
    }
  }
 
  Serial.println("-----------------------");
}

void publishPortStatusToMqtt(char* topic, int pin) {
  if (client.publish(topic, String(digitalRead(pin) ? "ON" : "OFF").c_str(), true)) {
    Serial.print("Publish status ");
    Serial.print(digitalRead(pin));
    Serial.print(" for pin ");
    Serial.println(pin);
    Serial.println("##############");
  } else {
    restartAllConnections();
  }
}

void setPortViaMqttTopicStatus(int port, char *topic, byte *payload, unsigned int length, bool pingIfChange, bool valueForPing) {
      if (!strncmp((char *)payload, "ON", length)) {
        if (pingIfChange) {
          digitalWrite(port, valueForPing);
            Serial.print("Set: ");
            Serial.println(valueForPing);
          delay(1000);
          digitalWrite(port, !valueForPing);
            Serial.print("Set: ");
            Serial.println(!valueForPing);

            Serial.print("PORT ");
            Serial.print(port);
            Serial.println(" pinged.");
        } else {
          digitalWrite(port, HIGH);
            Serial.print("PORT ");
            Serial.print(port);
            Serial.println(" set to HIGH.");
        }
      } else if (!strncmp((char *)payload, "OFF", length)) {
         if (pingIfChange) {
          digitalWrite(port, valueForPing);
          delay(1000);
          digitalWrite(port, !valueForPing);
            Serial.print("PORT ");
            Serial.print(port);
            Serial.println(" pinged.");
        } else {
          digitalWrite(port, LOW);
            Serial.print("PORT ");
            Serial.print(port);
            Serial.println(" set to LOW.");
        }
      }
}

// void connectWifi() {
//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.println("Connecting to WiFi..");
//   }
//   Serial.println("Connected to the WiFi network");
// }

void connectEthernet() {
  Ethernet.begin(mac, ip);
}

void connectMqtt() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
 
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (!client.connect("deviceId", mqttUser, mqttPassword )) {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  Serial.println("connected"); 

  for (int i =0; i < sizeof(mqttSubscribeTopics); i++) {
    client.subscribe(mqttSubscribeTopics[i]);
  }

  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    client.subscribe(settingsOutput[i].subscribedTopic);
  }
}

void restartAllConnections() {
      // Serial.println("Restart WiFi connection...");
      // WiFi.disconnect();
      // connectWifi();

      Ethernet.begin(mac, ip);

      Serial.println("Restart MQTT connection...");
      client.disconnect();
      connectMqtt();
}
