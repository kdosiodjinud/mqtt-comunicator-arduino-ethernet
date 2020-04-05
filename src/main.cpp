#include <Arduino.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <AsyncDelay.h>

// ethernet 
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
const char* hostname = "RelayBoard-1";

 // mqtt server
const char* mqttServer = "192.168.1.100";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "admin";
const char* mqttDeviceId = "RelayBoard-1";

// firmware
const char* hearthbeatTopic = "devices/arduino/relayboard-1/heartbeat";
const int ethCheckConnectionTimerMillis = 5000;
const int hearthbeatTimerMillis = 10000;
const int pingTimerMillis = 50;
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
bool infoOutputPinDelayForPing[99];
AsyncDelay OutputPinDelayForPing[99];

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
AsyncDelay delay_ethCheckConnection;
AsyncDelay delay_hearthbeat;

void connectEthernet();
bool connectMqtt();
void restartAllConnections();
void pushHearthbeat();
void prepareOutputPins();
void prepareInputPins();
void prepareTimers();
bool checkEthernetAndReconnectIfConnectionLost();
bool checkMqttConnectionAndReconnectIfConnectionLost();
void checkInputChangesAndPublishToMqtt();
void checkAsyncPingsForSetDefaultValue();
void pingPinAndSetAsyncTimer(int pin, bool valueForPing, int arrayIndex);
void callback(char* topic, byte* payload, unsigned int length);
void setPortViaMqttTopicStatus(int port, char *topic, byte *payload, unsigned int length, bool pingAndReturn, bool valueForPing, int arrayIndex);
void publishPortStatusToMqtt(char* topic, int port, bool publishTrueIfInputLow);

void setup() {
  Serial.begin(115200);

  prepareTimers();

  prepareOutputPins();
  prepareInputPins();

  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    infoOutputPinDelayForPing[i] = false;
  }

  connectEthernet();
  connectMqtt();
}

void loop() {
  checkAsyncPingsForSetDefaultValue();

  if (checkEthernetAndReconnectIfConnectionLost() && checkMqttConnectionAndReconnectIfConnectionLost()) {
    mqttClient.loop();
    pushHearthbeat();
    checkInputChangesAndPublishToMqtt();
  }
}

// MQTT subscribe
void callback(char* topic, byte* payload, unsigned int length) {
 
  Serial.println("We are in CALLBACK");
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for ( unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // ##### SUBSCRIBE TOPICS AND MAKE REACTION HERE #######
  Serial.println("Iterate on settingsOutput:");
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    if (strcmp(settingsOutput[i].subscribedTopic, topic) == 0) {
      Serial.print("- change in topic: ");
      Serial.println(topic);

      if (settingsOutputInitial[i]) {
        Serial.println("- settingsOutputInitial for pin exists!");
        Serial.println("- setPortViaMqttTopicStatus");
        Serial.print("- subscribed topic: ");
        Serial.println(settingsOutput[i].subscribedTopic);
        Serial.print("- value for ping: ");
        Serial.println(settingsOutput[i].valueForPing);

        setPortViaMqttTopicStatus(
          settingsOutput[i].pin,
          settingsOutput[i].subscribedTopic,
          payload,
          length,
          settingsOutput[i].pingAndReturn,
          settingsOutput[i].valueForPing,
          i
        );
      } else {
        Serial.println("- settingsOutputInitial for pin NOT exists! Creating!");
        settingsOutputInitial[i] = digitalRead(settingsOutput[i].pin);
      }
    }
  }
}

void publishPortStatusToMqtt(char* topic, int pin, bool publishTrueIfInputLow) {
  Serial.println("Start publishing port status to MQTT");

  bool pinValue = digitalRead(pin);

  if (publishTrueIfInputLow) {
    Serial.println("- invert value!");
    pinValue = pinValue ? false : true;
  }

  if (mqttClient.publish(topic, String(pinValue ? "ON" : "OFF").c_str(), true)) {
    Serial.print("- publish status: ");
    Serial.print(String(pinValue ? "ON" : "OFF").c_str());
    Serial.print(" for pin ");
    Serial.println(pin);
    Serial.println("##############");
  } else {
    Serial.print("Publishing failed! Restart all connections!!!");

    restartAllConnections();
  }
}

//todo: refactor method parameters
void setPortViaMqttTopicStatus(int pin, char *topic, byte *payload, unsigned int length, bool pingAndReturn, bool valueForPing, int arrayIndex) {
  if (pingAndReturn) {
    pingPinAndSetAsyncTimer(pin, valueForPing, arrayIndex);
  } else {
      Serial.print("PIN: ");
      Serial.print(pin);

      if (!strncmp((char *)payload, "ON", length)) {
        digitalWrite(pin, HIGH);
            
         Serial.println(" set to HIGH.");
      } else if (!strncmp((char *)payload, "OFF", length)) {
          digitalWrite(pin, LOW);
           
          Serial.println(" set to LOW.");
      } else {
        Serial.println(" unknown payload for process pins setup!");
      }
  }
}

void connectEthernet() {
  Serial.print("Connecting to ethernet network.");

  Ethernet.begin(mac);
}

bool connectMqtt() {

  bool connected = false;

  Serial.println("Connecting to MQTT start:");

  Serial.println("- reset previous status (settingsOutputInitial)");
  memset(settingsOutputInitial, 0, sizeof(settingsOutputInitial));

  Serial.print("- set server: ");
  Serial.print(mqttServer);
  Serial.print(":");
  Serial.println(mqttPort);
  mqttClient.setServer(mqttServer, mqttPort);

  Serial.println("- set MQTT callback");
  mqttClient.setCallback(callback);

  if (!mqttClient.connected()) {
    Serial.print("Connecting MQTT to server: ");

    if (!mqttClient.connect(mqttDeviceId, mqttUser, mqttPassword )) {
      Serial.print("- failed with state ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }

  if (!mqttClient.connected()) {
    Serial.println("- cannot connect, try later..."); 
  } else {
    Serial.println("- connected"); 
    connected = true;

    Serial.println("Set subscribe for all OUTPUT topics:");
    for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
      Serial.print(" - ");
      Serial.println(settingsOutput[i].subscribedTopic);

      mqttClient.subscribe(settingsOutput[i].subscribedTopic);
    }

    Serial.println("Connection and setup MQTT complete!");
  }

  return connected;
}

void restartAllConnections() {
      Serial.println("RESTARTING ALL CONNECTIONS");
      connectEthernet();

      Serial.println("Restart MQTT connection...");
      mqttClient.disconnect();
      
      connectMqtt();
}

void pushHearthbeat() {
  if (delay_hearthbeat.isExpired()) {
    Serial.print("Send hearthbeat to topic: ");
    Serial.println(hearthbeatTopic);

    mqttClient.publish(hearthbeatTopic, "ON", true);

    delay_hearthbeat.repeat();
  }
}

bool checkEthernetAndReconnectIfConnectionLost() {
  bool connected = true;

  if (delay_ethCheckConnection.isExpired()) {
    Serial.print("Check network connection: ");

    connected = false;
    if (!ethClient.connected()) {
        Serial.println("lost");

        Serial.println("Connection lost... Reconnecting...");
        connectEthernet();
    } else {
      Serial.println("ok");
    }

    delay_ethCheckConnection.repeat();
  }

  return connected;
}

bool checkMqttConnectionAndReconnectIfConnectionLost() {
  bool connected = true;

  if (!mqttClient.connected()) {
    Serial.println("Connection to MQTT server lost - reconnecting.");
    connected = connectMqtt();
  }

  return connected;
}

void prepareTimers() {
  Serial.print("Set timer for check ethernet connection to (miliseconds):");
  Serial.println(ethCheckConnectionTimerMillis);

  delay_ethCheckConnection.start(ethCheckConnectionTimerMillis, AsyncDelay::MILLIS);

  Serial.print("Set timer for sending heartbeat to (miliseconds):");
  Serial.println(hearthbeatTimerMillis);

  delay_hearthbeat.start(hearthbeatTimerMillis, AsyncDelay::MILLIS);

  Serial.println("Init timers for async ping values:");
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    Serial.print("- timer for ping to topic ");
    Serial.print(settingsOutput[i].subscribedTopic);
    Serial.println(" created");

    OutputPinDelayForPing[i] = AsyncDelay();
    OutputPinDelayForPing[i].start(pingTimerMillis, AsyncDelay::MILLIS);
  }

}

void prepareOutputPins() {
  Serial.println("Setting pin mode for OUTPUTS:");
  
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    Serial.print("- subscribe topic: ");
    Serial.println(settingsOutput[i].subscribedTopic);
    Serial.print("- pin: ");
    Serial.println(settingsOutput[i].pin);
    Serial.print("- logic: ");
    Serial.println(settingsOutput[i].defaultLogic);

    pinMode(settingsOutput[i].pin, OUTPUT);
    digitalWrite(settingsOutput[i].pin, settingsOutput[i].defaultLogic);
  }
}

void prepareInputPins() {
  Serial.println("Setting pin mode for INPUTS:");

  for (int i =0; i < sizeof(settingsInput) / sizeof(settingsInput[0]); i++) {
    Serial.print("- subscribe topic: ");
    Serial.println(settingsInput[i].publishToTopic);
    Serial.print("- pin: ");
    Serial.println(settingsInput[i].pin);
  
    pinMode(settingsInput[i].pin, INPUT);

    if (settingsInput[i].publishTrueIfInputLow) {
      Serial.print("- set as HIGH for enable pull-up");

      digitalWrite(settingsInput[i].pin, HIGH); // Enable pull-up rezistors
    }
  }
}

void checkInputChangesAndPublishToMqtt() {
  for (int i =0; i < sizeof(settingsInput) / sizeof(settingsInput[0]); i++) {
    if (digitalRead(settingsInput[i].pin) != settingsInputPrevious[i]) {
      Serial.println("SettingsInput have changes, publish PIN status to MQTT (publishPortStatusToMqtt):");
      Serial.print(" - topic: ");
      Serial.println(settingsInput[i].publishToTopic);
      Serial.print(" - pin: ");
      Serial.println(settingsInput[i].pin);
      Serial.print(" - publish true if input low: ");
      Serial.println(settingsInput[i].publishTrueIfInputLow);

      publishPortStatusToMqtt(settingsInput[i].publishToTopic, settingsInput[i].pin, settingsInput[i].publishTrueIfInputLow);

      Serial.print(" - set settingsInputPrevious:");
      Serial.println(settingsInput[i].pin);

      settingsInputPrevious[i] = digitalRead(settingsInput[i].pin);
    }
  }
}

void checkAsyncPingsForSetDefaultValue() {
  for (int i =0; i < sizeof(settingsOutput) / sizeof(settingsOutput[0]); i++) {
    if(OutputPinDelayForPing[i].isExpired() && infoOutputPinDelayForPing[i]) {
      Serial.print("Async ping expired, set pin ");
      Serial.print(settingsOutput[i].pin);
      Serial.print(" to default value ");
      Serial.print(!settingsOutput[i].valueForPing);
      Serial.print(" (topic: ");
      Serial.print(settingsOutput[i].subscribedTopic);
      Serial.println(")");

      digitalWrite(settingsOutput[i].pin, !settingsOutput[i].valueForPing);

      infoOutputPinDelayForPing[i] = false;
    }
  }
}

void pingPinAndSetAsyncTimer(int pin, bool valueForPing, int arrayIndex) {
  Serial.print("Ping pin ");
  Serial.print(pin);
  Serial.print(" with value ");
  Serial.print(valueForPing);
  Serial.println(" and restart timer for start async return to default");

  digitalWrite(pin, valueForPing);
  OutputPinDelayForPing[arrayIndex].restart();
  infoOutputPinDelayForPing[arrayIndex] = true;
}