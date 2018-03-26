#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HX711.h> 
#define DHTTYPE DHT22
#define DHTPIN 4
#define ONE_WIRE_BUS 3
HX711 scale(A1, A0); 

long Taragewicht = 8431132;  // Hier ist der Wert aus der Kalibrierung einzutragen
float Skalierung = 10.39;  // Hier ist der Wert aus der Kalibrierung einzutragen
 
long Gewicht = 999999;                      
long LetztesGewicht = 0;

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer = { 0x28, 0xFF, 0x24, 0x3F, 0x01, 0x16, 0x04, 0xF1 };

DHT dht(DHTPIN, DHTTYPE);

const char apn[]  = "internet.telekom";
const char user[] = "t-mobile";
const char pass[] = "tm";

#include <SoftwareSerial.h>
SoftwareSerial SerialAT(9, 8); 

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

const char* broker = "swarm.hiveeyes.org";
const char* topic = "hiveeyes/kasper/Garten/Stand1/data/Temperatur";
const char* topic1 = "hiveeyes/kasper/Garten/Stand1/data/DS18";
const char* topic2 = "hiveeyes/kasper/Garten/Stand1/data/Gewicht";
const char* clientID = "kasper";
const char* username = "xxxxx";
const char* passwort = "xxxx";

String temp_str;
char temp[50];
String temp1_str;
char temp1[50];
String temp2_str;
char temp2[50];


long lastReconnectAttempt = 0;

void setup_gsm() {
  
  SerialAT.begin(9600);
  delay(3000);
  modem.restart();
  String modemInfo = modem.getModemInfo();
  if (!modem.waitForNetwork()) {
    while (true);
  }
  if (!modem.gprsConnect(apn, user, pass)) {
    while (true);
  }
 }

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    if (mqtt.connect(clientID, username, passwort)) {
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      setup_gsm();
      delay(5000);
    }
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("Setupstart");
  pinMode(7, OUTPUT);
  delay(100);
  digitalWrite(7, LOW);
  delay(100);
  dht.begin();
  sensors.begin();
  sensors.setResolution(insideThermometer, 12);
  scale.set_offset(Taragewicht);
  scale.set_scale(Skalierung);

  // Set console baud rate
  Serial.begin(9600);
  delay(10);
  setup_gsm();

  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  
  Serial.println("SetupEnde");
}

boolean mqttConnect() {
  if (!mqtt.connect(clientID, username, passwort)) {
    return false;
  }
  return mqtt.connected();
}
void loop() {
    Serial.println("LoopStart");
    if (!mqtt.connected()) {
       reconnect();
    }
               
    delay(15000); 
    float tempdht = dht.readTemperature();
    sensors.requestTemperatures();
    float tempc = sensors.getTempC(insideThermometer);

    for(byte j=0 ;j < 3; j++) { // Anzahl der Widerholungen, wenn Abweichung zum letzten Gewicht zu hoch
     Gewicht= scale.get_units(10);
     if ((Gewicht-LetztesGewicht < 500) and (Gewicht-LetztesGewicht > -500)) j=10;  // Abweichung für Fehlererkennung
     if (j < 3) delay(3000); // Wartezeit zwischen Wiederholungen
    } 
    LetztesGewicht = Gewicht;

    temp_str = String(tempdht);
    temp_str.toCharArray(temp, temp_str.length() + 1); 
    mqtt.publish(topic, temp);

    temp1_str = String(tempc);
    temp1_str.toCharArray(temp1, temp1_str.length() + 1); 
    mqtt.publish(topic1, temp1); 

    temp2_str = String(Gewicht);
    temp2_str.toCharArray(temp2, temp2_str.length() + 1); 
    mqtt.publish(topic2, temp2);   
    
 }
void mqttCallback(char* topic, byte* payload, unsigned int lenght) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, lenght);
  Serial.println();
}
