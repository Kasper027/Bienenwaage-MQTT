#include <SPI.h>


//----------------------------------------------------------------
// Konfiguration SIM800L - Mobilfunk
//----------------------------------------------------------------




//----------------------------------------------------------------


//----------------------------------------------------------------
// Konfiguration DS18B20 - Temperatur
//----------------------------------------------------------------

#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 3

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer = { 0x28, 0xFF, 0x24, 0x3F, 0x01, 0x16, 0x04, 0xF1 };
float tempc = 0.00;


//----------------------------------------------------------------



//----------------------------------------------------------------
// Konfiguration DHT21 / DHT22 - Temperatur und Luftfeuchte
//----------------------------------------------------------------
#include "DHT.h"
#define DHTTYPE DHT22
#define DHTPIN 4
DHT dht(DHTPIN, DHTTYPE);
float tempdht =0.00;


//----------------------------------------------------------------



//----------------------------------------------------------------
// Konfiguration Gewicht
//----------------------------------------------------------------
#include <HX711.h> 
HX711 scale(A1, A0); 
long Taragewicht = 8431132;  // Hier ist der Wert aus der Kalibrierung einzutragen
float Skalierung = 10.39;  // Hier ist der Wert aus der Kalibrierung einzutragen
long Gewicht = 999999;                      
long LetztesGewicht = 0;
float Kalibriertemperatur = 0;       // Temperatur zum Zeitpunkt der Kalibrierung
float KorrekturwertGrammproGrad = 0; // Korrekturwert zur Temperaturkompensation - '0' für Deaktivierung

int Kalib_Spannung = 0;  // Hier muss der Wert aus der Kalibrierung eingetragen werden, sonst funktioniert der Programmcode nicht
int Kalib_Bitwert = 0;   // Hier muss der Wert aus der Kalibrierung eingetragen werden, sonst funktioniert der Programmcode nicht


//----------------------------------------------------------------



//----------------------------------------------------------------
// Konfiguration RTC & Sleep-Mode
//----------------------------------------------------------------

byte WeckIntervallMinuten = 1;        
byte WeckIntervallStunden = 0; 

#include <Sodaq_DS3231.h>
#include <LowPower.h>
//#include <Wire.h>

byte DS3231_Interrupt_Pin=2;
byte Power_Pin=23;

volatile bool ok_sleep = false;
//RTC_DS3231 RTC;

//----------------------------------------------------------------



//----------------------------------------------------------------
// Konfiguration MQTT
//----------------------------------------------------------------
#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include <PubSubClient.h>

TinyGsm modem(Serial1);
TinyGsmClient client(modem);
PubSubClient mqtt(client);


const char* broker = "swarm.hiveeyes.org";
const char* top  = "hiveeyes/kasper/Garten/Stand1/data/";
const char* topic = "Temperatur";
const char* topic1 = "DS18";
const char* topic2 = "Gewicht";
const char* clientID = "xxx";
const char* username = "xxxxx";
const char* passwort = "xxxxx";

String temp_str;
char temp[50];
String temp1_str;
char temp1[50];
String temp2_str;
char temp2[50];

//----------------------------------------------------------------



//----------------------------------------------------------------
// Konfiguration GSM
//----------------------------------------------------------------

const char apn[]  = "internet.telekom";
const char user[] = "t-mobile";
const char pass[] = "tm";

long lastReconnectAttempt = 0;

//----------------------------------------------------------------


void setup() {
//----------------------------------------------------------------
// Setup RTC & Sleep-Mode
//----------------------------------------------------------------
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH); 
  
  delay(50);
  //Wire.begin();
  rtc.begin();
  Serial.begin(115200);

  DateTime pc_tim = DateTime(__DATE__, __TIME__);  
  long l_pczeit = pc_tim.get();
  DateTime aktuell = rtc.now();                    
  long l_zeit = aktuell.get();
  if (l_pczeit > l_zeit)  rtc.setDateTime(pc_tim.get());

  rtc.clearINTStatus();

  pinMode(2, INPUT_PULLUP);
  ok_sleep = true; 
  
  delay(50);


//----------------------------------------------------------------


//----------------------------------------------------------------
// Setup SIM800L - Mobilfunk
//----------------------------------------------------------------    
  pinMode(22, OUTPUT);
  digitalWrite(22, LOW);   
//----------------------------------------------------------------  

}


void loop() {
  Sensor_DS18B20();
  Sensor_DHT();
  Sensor_Gewicht();
  Senden_GSM();
  Alarm_konfigurieren();
  SleepNow();
}


//----------------------------------------------------------------
// Funktion Sensor_DS18B20
//----------------------------------------------------------------
void Sensor_DS18B20(){
  digitalWrite(22, LOW);
  delay(1500);
  Serial.begin(115200);
  sensors.begin();
  sensors.setResolution(insideThermometer, 12);

  sensors.requestTemperatures();
  tempc = sensors.getTempC(insideThermometer);
  Serial.println(tempc);
  }

//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion Sensor_DHT
//----------------------------------------------------------------
void Sensor_DHT(){
  delay(500);
  dht.begin();
  tempdht = dht.readTemperature();
  Serial.println(tempdht);
  }

//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion Sensor_Gewicht
//----------------------------------------------------------------
void Sensor_Gewicht(){
  delay(500);
  Serial.println(Taragewicht);
  scale.set_offset(Taragewicht);
  scale.set_scale(Skalierung);

  LowPower.powerStandby(SLEEP_500MS,ADC_OFF, BOD_OFF);
    for(byte j=0 ;j < 3; j++) { // Anzahl der Widerholungen, wenn Abweichung zum letzten Gewicht zu hoch
      Gewicht= scale.get_units(10);
      if ((Gewicht-LetztesGewicht < 500) and (Gewicht-LetztesGewicht > -500)) j=10; // Abweichung für Fehlererkennung
      if (j < 3) {
        delay(500);
        LowPower.powerStandby(SLEEP_2S,ADC_OFF, BOD_OFF);  // Wartezeit zwischen Wiederholungen 
        LowPower.powerStandby(SLEEP_1S,ADC_OFF, BOD_OFF);  // Wartezeit zwischen Wiederholungen      
      }
    } 
   /* // Temperaturkompensation
    if ((tempc != 999.99)){
      if (tempc > Kalibriertemperatur) Gewicht = Gewicht-(fabs(tempc-Kalibriertemperatur)*KorrekturwertGrammproGrad); 
      if (tempc < Kalibriertemperatur) Gewicht = Gewicht+(fabs(tempc-Kalibriertemperatur)*KorrekturwertGrammproGrad);
    } 
    */
    LetztesGewicht = Gewicht;
    Serial.println(Gewicht);
    delay(500);
    //scale.power_down();




}

//----------------------------------------------------------------


//----------------------------------------------------------------
// Funktion Senden_GSM    

// Hier drinn sind noch einige unterfunktionen. Der Übersicht sind aber alle in dieser Gruppe
//----------------------------------------------------------------

//Wird das hier überhaupt benötigt? Für den ersten Test drinn lassen. Ansonsten muss dieses raus.
boolean mqttConnect() {
  if (!mqtt.connect(clientID, username, passwort)) {
    return false;
  }
  return mqtt.connected();
}

//----------------------------------------------------------------
// Funktion GSM Verbindung aufbauen    
//----------------------------------------------------------------
void setup_gsm() {
  Serial.println("Void setup_gsm gestartet");
  Serial1.begin(9600);
  delay(3000);
  modem.restart();
  Serial.println("Modem Restart");
  String modemInfo = modem.getModemInfo();
  if (!modem.waitForNetwork()) {
    while (true);
  }
  if (!modem.gprsConnect(apn, user, pass)) {
    while (true);
  }
 }
//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion Verbindung erneuern, falls es Probleme gibt.  
//----------------------------------------------------------------
/*void reconnect() {
  // Loop until we're reconnected
  Serial.println("Void reconnect gestartet");
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
*/
//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion Verbindung aufbauen, Vorbereitung der Daten, Daten senden 
//----------------------------------------------------------------

void Senden_GSM(){
  Serial.println("Void Senden_GSM gestartet");
	setup_gsm();
  Serial.println("MQTT.setServer");
	mqtt.setServer(broker, 1883);
	//mqtt.setCallback(mqttCallback);   //kann beim Entfernen der unterern Ecke mit raus
	if (!mqtt.connected()) {
      Serial.println("Void reconnect Aufgerufen");
     while (!mqtt.connected()) {
        if (mqtt.connect(clientID, username, passwort)) {
          return mqtt.connected();
          Serial.println("mqtt.connect ist ok");
        } else {
          Serial.print("failed, rc=");
          Serial.print(mqtt.state());
          setup_gsm();
          delay(5000);
        }
      }
    }

	  temp_str = String(tempdht);
    temp_str.toCharArray(temp, temp_str.length() + 1); 
    mqtt.publish(strcat(top,topic), temp);

    temp1_str = String(tempc);
    temp1_str.toCharArray(temp1, temp1_str.length() + 1); 
    mqtt.publish(strcat(top,topic1), temp1);

    temp2_str = String(Gewicht);
    temp2_str.toCharArray(temp2, temp2_str.length() + 1); 
    mqtt.publish(strcat(top,topic2), temp2);
	  Serial.println("MQTT gesendet");
	  delay(500);



	  digitalWrite(22, HIGH);
	  delay(1000);
}
//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion für Eingehende Nachrichten vom MQTT Broker  --> Kann eigentlich raus
//----------------------------------------------------------------

/*
void mqttCallback(char* topic, byte* payload, unsigned int lenght) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, lenght);
  Serial.println();
}
*/
//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion Alarm_konfigurieren
//----------------------------------------------------------------
void Alarm_konfigurieren(){
 
  byte Stunde,IntervallStunden; 
  byte Minute,IntervallMinuten;
  IntervallStunden = WeckIntervallStunden;
  IntervallMinuten = WeckIntervallMinuten;

  DateTime aktuell = rtc.now();
  long timestamp = aktuell.get(); 
  aktuell = timestamp + IntervallStunden*3600 + IntervallMinuten*60;    
  rtc.enableInterrupts(aktuell.hour(), aktuell.minute(),0);
}

//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion SleepNow
//----------------------------------------------------------------
void SleepNow(){
 delay(5);  
  
  digitalWrite(23, LOW);
  delay(5);
    
  //digitalWrite (A4, LOW);
  //digitalWrite (A5, LOW);
  //digitalWrite (22, HIGH);
  //digitalWrite (8, LOW);
  //digitalWrite (9, LOW);
  //digitalWrite (10, LOW);
  digitalWrite (ONE_WIRE_BUS, LOW);
  digitalWrite (DHTPIN, LOW);
  //digitalWrite (DHT_Sensor_Pin[1], LOW);
  //digitalWrite (A0, LOW);
  //digitalWrite (A1, LOW);
  
  attachInterrupt(0,WakeUp,LOW);
  TWCR &= ~(bit(TWEN) | bit(TWIE) | bit(TWEA)); 
  if (ok_sleep) {
    LowPower.powerDown(SLEEP_FOREVER,ADC_OFF, BOD_OFF);
  }
  digitalWrite (22, HIGH);
  //digitalWrite (8, HIGH);
  //digitalWrite (A0, HIGH);
  //digitalWrite (A1, HIGH);// SCK = A0
  digitalWrite (DHTPIN, HIGH);
  //digitalWrite (DHT_Sensor_Pin[1], HIGH);
  
  digitalWrite(23, HIGH);
  delay (100);
  //Wire.begin();
  rtc.begin();
  rtc.clearINTStatus();
  while(digitalRead(DS3231_Interrupt_Pin)==false){};
  ok_sleep = true;
  
  delay(100);
}  

//----------------------------------------------------------------

//----------------------------------------------------------------
// Funktion WakeUp
//----------------------------------------------------------------
void WakeUp() {
  ok_sleep = false; 
  detachInterrupt(0);
}
//----------------------------------------------------------------
