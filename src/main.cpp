#include <Arduino.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <AsyncDelay.h>
// #include <ESP8266WiFi.h>

 //############ CONFIGURATION ################
 // wifi
// const char* ssid = "NazevSite";
// const char* password =  "heslo";

// ethernet 
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
const char* hostname = "RelayBoard1";

 // mqtt server
const char* mqttServer = "192.168.1.100";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "admin";
//#############################################


struct settingsOutput {
  char* subscribedTopic;
  int pin;
  bool defaultLogic;
  bool pingAndReturn;
  bool valueForPing;
} settingsOutput[16] = {
  // SVETLA
  { "dum/pracovna/svetla/hlavni", 22, HIGH, true, LOW }, // PRACOVNA
  { "dum/technicka/svetla/hlavni", 23, HIGH, true, LOW }, // TECHNICKA
  { "dum/predsin/svetla/hlavni", 24, HIGH, true, LOW }, // PREDSIN
  { "dum/dolnichodba/svetla/hlavni", 25, HIGH, true, LOW }, // DOLNI CHODBA
  { "dum/spiz/svetla/hlavni", 26, HIGH, true, LOW }, // SPIZ
  { "dum/dolnikoupelna/svetla/hlavni", 27, HIGH, true, LOW }, // DOLNI KOUPELNA
  { "dum/kuchyn/svetla/hlavni", 28, HIGH, true, LOW }, // KUCHYN
  { "dum/jidelna/svetla/hlavni", 29, HIGH, true, LOW }, // JIDELNA
  { "dum/obyvak/svetla/hlavni", 30, HIGH, true, LOW }, // OBYVAK
  { "dum/schody/svetla/hlavni", 31, HIGH, true, LOW }, // SCHODY
  { "dum/hornichodba/svetla/hlavni", 32, HIGH, true, LOW }, // HORNI CHODBA
  { "dum/hornikoupelna/svetla/hlavni", 33, HIGH, true, LOW }, // HORNI KOUPELNA
  { "dum/koupelna/svetla/hlavni", 34, HIGH, true, LOW }, // ZRDCADLA
  { "dum/loznice/svetla/hlavni", 35, HIGH, true, LOW }, // LOŽNICE

  // ZASUVKY
  { "dum/kuchyn/zasuvky/drezLeva", 36, HIGH, true, LOW }, // ZASUVKY KUCHYN 1
  { "dum/hornichodba/zasuvky/uprostred", 37, HIGH, true, LOW } // ZASUVKA HORNI CHODBA
};

struct settingsInput {
  char* publishToTopic;
  int pin;
  bool publishTrueIfInputLow;
} settingsInput[0] = {
  // SVETLA
  // { "dum/pracovna/svetla/hlavniPotvrzeni", 38, true}, // PRACOVNA
  // { "dum/technicka/svetla/hlavniPotvrzeni", 39, true}, // TECHNICKA
  // { "dum/predsin/svetla/hlavniPotvrzeni", 40, true}, // PREDSIN
  // { "dum/dolnichodba/svetla/hlavniPotvrzeni", 41, true}, // DOLNI CHODBA
  // { "dum/spiz/svetla/hlavniPotvrzeni", 42, true}, // SPIZ
  // { "dum/dolnikoupelna/svetla/hlavniPotvrzeni", 43, true}, // DOLNI KOUPELNA
  // { "dum/kuchyn/svetla/hlavniPotvrzeni", 44, true}, // KUCHYN
  // { "dum/jidelna/svetla/hlavniPotvrzeni", 45, true}, // JIDELNA
  // { "dum/obyvak/svetla/hlavniPotvrzeni", 46, true}, // OBYVAK
  // { "dum/schody/svetla/hlavniPotvrzeni", 47, true}, // SCHODY
  // { "dum/hornichodba/svetla/hlavniPotvrzeni", 48, true}, // HORNI CHODBA
  // { "dum/hornikoupelna/svetla/hlavniPotvrzeni", 49, true}, // HORNI KOUPELNA
  // { "dum/koupelna/svetla/hlavniPotvrzeni", 50, true}, // ZRDCADLA
  // { "dum/loznice/svetla/hlavniPotvrzeni", 51, true}, // LOŽNICE

  // ZASUVKY
  // { "dum/kuchyn/zasuvky/drezLevaPotvrzeni", 52, true}, // ZASUVKY KUCHYN 1
  // { "dum/hornichodba/zasuvky/uprostredPotvrzeni", 53, true} // ZASUVKA HORNI CHODBA
};


int settingsOutputInitial[99];
int settingsInputPrevious[99];


// WiFiClient espClient;
EthernetClient ethClient;
PubSubClient client(ethClient);
AsyncDelay delay_1s;
AsyncDelay delay_ethCheckConnection;

// main methods definitions
void connectWifi();
void connectEthernet();
void connectMqtt();
void restartAllConnections();
void callback(char* topic, byte* payload, unsigned int length);
void setPortViaMqttTopicStatus(int port, char *topic, byte *payload, unsigned int length, bool pingAndReturn, bool valueForPing);
void publishPortStatusToMqtt(char* topic, int port, bool publishTrueIfInputLow);

// master setup method
void setup() {
  delay(1000);
  delay_1s.start(1000, AsyncDelay::MILLIS);
  delay_ethCheckConnection.start(5000, AsyncDelay::MILLIS);
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
  
    if (settingsInput[i].publishTrueIfInputLow) {
      digitalWrite(settingsInput[i].pin, HIGH); // Enable pull-up rezistors
    }
  }

  // connectWifi();
  connectEthernet();
  connectMqtt();
}

void loop() {

  if (delay_ethCheckConnection.isExpired()) {
    if (!ethClient.connected()) {
        Serial.println("Connection lost... Reconnecting...");
        connectEthernet();
        connectMqtt();
    }

    delay_ethCheckConnection.repeat();
  }

  client.loop();

  //todo: načíst z MQTT previous status, než se začne publishovat (nebo to vyřeší hassio tím, že bude znát aktuální stav? - zkusit)

  for (int i =0; i < sizeof(settingsInput) / sizeof(settingsInput[0]); i++) {
    if (digitalRead(settingsInput[i].pin) != settingsInputPrevious[i]) {
      publishPortStatusToMqtt(settingsInput[i].publishToTopic, settingsInput[i].pin, settingsInput[i].publishTrueIfInputLow);
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
      if (settingsOutputInitial[i]) {
        setPortViaMqttTopicStatus(settingsOutput[i].pin, settingsOutput[i].subscribedTopic, payload, length, settingsOutput[i].pingAndReturn, settingsOutput[i].valueForPing);
      } else {
        settingsOutputInitial[i] = digitalRead(settingsOutput[i].pin);
      }
    }
  }
 
  Serial.println("-----------------------");
}

void publishPortStatusToMqtt(char* topic, int pin, bool publishTrueIfInputLow) {

  bool pinValue = digitalRead(pin);

  if (publishTrueIfInputLow) {
    pinValue = pinValue ? false : true;
  }

  if (client.publish(topic, String(pinValue ? "ON" : "OFF").c_str(), true)) {
    Serial.print("Publish status ");
    Serial.print(digitalRead(pin));
    Serial.print(" for pin ");
    Serial.println(pin);
    Serial.println("##############");
  } else {
    restartAllConnections();
  }
}

void setPortViaMqttTopicStatus(int port, char *topic, byte *payload, unsigned int length, bool pingAndReturn, bool valueForPing) {
      if (!strncmp((char *)payload, "ON", length)) {
        if (pingAndReturn) {
          digitalWrite(port, valueForPing);
            Serial.print("Set: ");
            Serial.println(valueForPing);
          delay(100); //todo: predělat na async
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
         if (pingAndReturn) {
          digitalWrite(port, valueForPing);
          delay(100);
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
  Ethernet.begin(mac);
}

void connectMqtt() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (!client.connect("deviceId", mqttUser, mqttPassword )) {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(5000);
    }
  }
  Serial.println("connected"); 

  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    client.subscribe(settingsOutput[i].subscribedTopic);
  }
}

void restartAllConnections() {
      // Serial.println("Restart WiFi connection...");
      // WiFi.disconnect();
      // connectWifi();

      Ethernet.begin(mac);

      Serial.println("Restart MQTT connection...");
      client.disconnect();
      connectMqtt();
}
