#include <Arduino.h>

/*------------------------------------------------------------------------------------------------------------------------------------------------------------------
  Program:      Füllstandsmessung mit Pegelsonde

  Description:  Dieses Programm misst die analolge Spannung an einem Port, welche von einem Drucksensor
                0-5m Meßhöhe und 4-20mA Stromschnittstelle erzeugt wird.
                Voraussetzung ist ein Meßwandler, welcher die 24V Versorgungsspannung an
                den Drucksensor liefert und 0-3,3V analoge Spannung ausgibt.
                Dienste:
                DHCP, wenn vorhanden, sonst wird eine feste IP mit 192.168.1.21 vergeben.
                  
  Hardware:     Arduino Nano ATmega 328P 
                W5100 Netzwerk shield
                LCD Display I2C HD44780
				
				Pin-Ports:
				A0 = Analog IN
				A4 = SDA
				A5 = SCL
				
  
  Date:         20191013
  Modified:     Initial Version 1.0
                - DHCP error routine

                20191017
                - Fixed uptime_d

                20191019
                Version 1.1
                - Dichte Berechnung hinzugefügt
                Version 1.2
                - LCD wechselt jetzt alle 30 Sek. zwischen Uptime und Analog Messwert

                20191024
                - Fixed MQTT uptime_d char array

                20191028
                Version 1.3
                - MQTT Port definierbar
                - MQTT User Password authentication
				
				20200110
				Version 1.4
				- Array für die Beruhigung des Messwertes eingefügt

        20200528 (ltathome)
        Version 1.5
        - Erweiterung MQTT für die Steuerung eines Relais für die Frischwasser Nachspeisung bzw. Umschaltung
        - Grenzwerte über MQTT einstellbar
        - Umschaltung Betriebsart über Taster Pin2 möglich

        20221004 (Eisbaeeer)
        Version 1.6
        - Code aufgeräumt
        - Portierung von Arduino IDE zu Platformio

  Author:       Eisbaeeer, https://github.com/Eisbaeeer               
 
  Author:       Ethernet part: W.A. Smith, http://startingelectronics.com
                progress bar -  CC BY-SA 3.0 :  skywodd, https://www.carnetdumaker.net/articles/faire-une-barre-de-progression-avec-arduino-et-liquidcrystal/
                ltathome fixes + parameter level switch
				
  LICENSE: 		MIT License
  
-------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Include some libraries
#include <Ethernet.h>
#include <SPI.h>

#include <Wire.h>
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header
hd44780_I2Cexp lcd; // declare lcd object: auto locate & auto config expander chip
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
const int LCD_COLS = 16;
const int LCD_ROWS = 4;
const int LCD_NB_COLUMNS = 16;

#include "progress.h"
#include <PubSubClient.h>



// ##############################################################################################################################
// ---- HIER die Anpassungen vornehmen ----
// ##############################################################################################################################
// Hier die maximale Füllmenge des Behälters angeben. Dies gilt nur für symmetrische Behälter.
const float max_liter = 6300;                           

// Analoger Wert bei maximalem Füllstand (wird alle 30 Sekungen auf dem LCD angezeigt oder in der seriellen Konsole mit 9600 Baud.
const int analog_max = 763;                           

// Dichte der Flüssigkeit - Bei Heizöl bitte "1.086" eintragen, wenn die Kalibrierung mit Wasser erfolgt ist! 
// Default ist 1.0 für Wasser
const float dichte = 1.0;                              

// IP Adresse und Port des MQTT Servers
IPAddress mqttserver(192, 168, 1, 200);
const int mqttport = 1883;
// Wenn der MQTT Server keine Authentifizierung verlangt, bitte folgende Zeile auskommentieren!
#define mqttauth

char *mqttuser = const_cast<char*>("MQTT ID");
char *mqttpass = const_cast<char*>("MQTTPASS");                 

// IP Adresse, falls kein DHCP vorhanden ist. Diese Adresse wird nur verwendet, wenn der DHCP-Server nicht erreichbar ist.
IPAddress ip(192, 168, 1, 21);                          

// MAC-Addresse bitte anpassen! Sollte auf dem Netzwerkmodul stehen. Ansonsten eine generieren.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0A };    

// ##############################################################################################################################
// AB hier nichts mehr ändern!
// (Ausser ihr wisst, was ihr tut)
// ##############################################################################################################################

char buf[40];
#define MODE_PIN 2
#define MODE_ZISTERNE 0
#define MODE_HAUSWASSER 1
#define MODE_AUTO 2
#define MODE_COUNT 10
bool setmode = false;
const char *modes[] = {"Zisterne", "Hauswasser", "Auto"};
int mode_count = MODE_COUNT;
int mode = MODE_AUTO;
int old_mode = MODE_AUTO;
int pin_stat;
int old_stat;

#define VALVE_PIN 3
#define VALVE_ZISTERNE LOW
#define VALVE_HAUSWASSER HIGH
int valve = VALVE_ZISTERNE;
int new_valve = VALVE_ZISTERNE;
int reason = 0;
const char *valves[] = {"Zisterne", "Hauswasser"};
const char *reasons[] = {"manuelle Steuerung", "Automatik voll", "Automatik leer"};
 
#define USE_SERIAL Serial

// No more delay
unsigned long startMillis;  // some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long sekunde = 1000;  // one seconds

unsigned long pinMillis;
unsigned long lastMillis = 0;

int secs = 0, mins = 0, hours = 0, days = 0;
char uptime[25];

float percent;
float liter;
float limit_low = 500;
float limit_high = 1000;
boolean LCD_Page;

// Analog IN
const int analogPin = A0;
const int messungen = 60;     // Anzahl Messungen
int myArray[messungen];       // Array für Messwerte
float analog = 0.0;             // Durchschnittswert
int pointer = 0;              // Pointer für Messung

// MQTT global vars
int send_interval = 30; // MQTT Sendeintervall in Sekunden (zulässing nur von 1-59 !)

boolean mqttconnected = false;

// MQTT definitions
void MqttCallback(char *topic, byte *payload, unsigned int length);
EthernetClient ethClient;
PubSubClient mqttclient;
#define MQTT_ID "Zisterne"
char pl[30];

void defaultEthernet(void) {
    Ethernet.begin(mac, ip);  // initialize Ethernet device
}

void MqttConnect(char* user, char* pass) {

    mqttclient.setClient(ethClient);
    mqttclient.setServer(mqttserver, mqttport);
    mqttclient.setCallback(MqttCallback);
    
    mqttconnected = mqttclient.connect(MQTT_ID, user, pass);
    if (mqttconnected) {
        // USE_SERIAL.println("Connected to Mqtt-Server");
        mqttclient.subscribe("Zisterne/cmnd/Mode");
        mqttclient.subscribe("Zisterne/cmnd/Limit");
        mqttclient.subscribe("Zisterne/Mode");
        // USE_SERIAL.println("subscribing to Zisterne/cmnd/Mode");
    }
}

void Mqttpublish(void) {
    if (mqttclient.connected()) {
        dtostrf(analog, 5, 2, buf);
        mqttclient.publish("Zisterne/Analog", buf);
        dtostrf(liter, 5, 0, buf);
        mqttclient.publish("Zisterne/Liter", buf);
        dtostrf(limit_low, 1, 0, buf);
        mqttclient.publish("Zisterne/LiterLow", buf);
        dtostrf(limit_high, 1, 0, buf);
        mqttclient.publish("Zisterne/LiterHigh", buf);
        dtostrf(percent, 5, 0, buf);
        mqttclient.publish("Zisterne/Prozent", buf);
        if (mode == 2) {
            mqttclient.publish("Zisterne/Modus", "Auto");
        } else {
            mqttclient.publish("Zisterne/Modus", "Manuell");          
        }
        dtostrf(mode, 1, 0, buf);
        mqttclient.publish("Zisterne/Mode", buf);
        dtostrf(valve, 1, 0, buf);
        mqttclient.publish("Zisterne/Valve", buf);
        mqttclient.publish("Zisterne/Ventil", valves[valve]);
        mqttclient.publish("Zisterne/Uptime", uptime);
    } else {
        MqttConnect(mqttuser, mqttpass);  
    }
}
 

void Uptime() {
    secs++;
    secs = secs % 60;
    if (secs == 0) {
        mins++;
        mins = mins % 60;
        if (mins == 0) {
            hours++;
            hours = hours % 24;
            if (hours == 0) {
                days++;
                days = days % 10000;   // Nach 9999 Tagen zurück auf 0 (das sind 27 Jahre....)
            }
        }
    }
    sprintf(uptime, "%4dd %2dh %2dm", days, hours, mins);
  
    if (secs == send_interval) { 
        LCD_Page = !LCD_Page;
        Mqttpublish();
    }
}

void ReadAnalog() {
    // read the analog value and build floating middle
    myArray[pointer++] = analogRead(analogPin);      // read the input pin
    // myArray[pointer++] = 352;
 
    pointer = pointer % messungen;

    // Werte aufaddieren
    for (int i = 0; i < messungen; i++) {
        analog = analog + myArray[i];
  }
    // Summe durch Anzahl - geglättet
    analog = analog / messungen;
 
    percent = min(100 * analog / analog_max, 100);
    float calc = max_liter / analog_max;      // calculate percent
    calc = calc * dichte;                     // calculate dichte
    liter = min(analog * calc, max_liter);    // calculate liter
}

void setup()
{
  USE_SERIAL.begin(9600);       // for debugging

/*--------------------------------------------------------------
 * LCD init
--------------------------------------------------------------*/
  lcd.init();
  int status;
  status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) // non zero status means it was unsuccesful
  {
    status = -status; // convert negative status value to positive number
    // begin() failed so blink error code using the onboard LED if possible
    hd44780::fatalError(status); // does not return
  }

  // initalization was successful, the backlight should be on now
  // Print a message to the LCD
  lcd.print("Zisterne");
  lcd.setCursor(0, 1);
  lcd.print("Version 1.5");
  lcd.setCursor(0, 3);
  lcd.print("github/Eisbaeeer");
  delay(2000);
  setup_progressbar();

/*--------------------------------------------------------------  
 * Milliseconds start
--------------------------------------------------------------*/
  startMillis = millis();  //initial start time

/*--------------------------------------------------------------  
 * Ethernet init
--------------------------------------------------------------*/  
    if (Ethernet.begin(mac) == 0) {
    // USE_SERIAL.println(F("Failed config using DHCP"));
    // DHCP not working, switch to static IP
    defaultEthernet();
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      // USE_SERIAL.println(F("Eth shield not found"));
    } else if (Ethernet.linkStatus() == LinkOFF) {
      // USE_SERIAL.println(F("Eth cable not conn"));
      }
    }
 
    /*-------------------------------------------------------------------
     * Setup Pins for Valve and mode-setting
     */
    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(VALVE_PIN, OUTPUT);
  
    pin_stat = old_stat = digitalRead(MODE_PIN);
}



void MqttCallback(char *topic, byte *payload, unsigned int length) {
    char *payloadvalue;
    char *payloadkey;

    payload[length] = '\0';
    payloadkey = (char *)&payload[0];

    // USE_SERIAL.println(payloadstring);
    if (strcmp(topic, "Zisterne/cmnd/Mode") == 0 || strcmp(topic, "Zisterne/Mode") == 0) {
        if (strcmp(payloadkey, "0") == 0 || strcmp(payloadkey, "Zisterne") == 0) {
            mode = MODE_ZISTERNE;
        } else if (strcmp(payloadkey, "1") == 0 || strcmp(payloadkey, "Hauswasser") == 0) {
            mode = MODE_HAUSWASSER;
        } else if (strcmp(payloadkey, "2") == 0 || strcmp(payloadkey, "Auto") == 0) {
            mode = MODE_AUTO;
        }
    } else if (strcmp(topic, "Zisterne/cmnd/Limit") == 0) {
        int eq = 0;
        for (unsigned int i = 0; i < length; i++) {
            if (payload[i] == '=') {
                eq = i;
                break;
            }
        }
        if (eq > 0) {
            payload[eq++] = 0;
            payloadvalue = (char *)&payload[eq];
            if (strcmp((char *)payload, "Low") == 0 || strcmp((char *)payload, "low") == 0) {
                limit_low = atoi(payloadvalue);
            }
            if (strcmp((char *)payload, "High") == 0 || strcmp((char *)payload, "high") == 0) {
                limit_high = atoi(payloadvalue);
            }
        }
    }
}

void draw_progressbar(byte percent) {
 
  lcd.clear(); 
  /* Affiche la nouvelle valeur sous forme numérique sur la première ligne */
  lcd.setCursor(0, 0);
  lcd.print(percent);
  lcd.print(F(" %  ")); 
  // N.B. Les deux espaces en fin de ligne permettent d'effacer les chiffres du pourcentage
  // précédent quand on passe d'une valeur à deux ou trois chiffres à une valeur à deux ou un chiffres.
 
  /* Déplace le curseur sur la seconde ligne */
  lcd.setCursor(0, 1);
 
  /* Map la plage (0 ~ 100) vers la plage (0 ~ LCD_NB_COLUMNS * 2 - 2) */
  byte nb_columns = map(percent, 0, 100, 0, LCD_NB_COLUMNS * 2 - 2);
  // Chaque caractère affiche 2 barres verticales, mais le premier et dernier caractère n'en affiche qu'une.

  /* Dessine chaque caractère de la ligne */
  for (byte i = 0; i < LCD_NB_COLUMNS; ++i) {

    if (i == 0) { // Premiére case

      /* Affiche le char de début en fonction du nombre de colonnes */
      if (nb_columns > 0) {
        lcd.write(1); // Char début 1 / 1
        nb_columns -= 1;

      } else {
        lcd.write((byte) 0); // Char début 0 / 1
      }

    } else if (i == LCD_NB_COLUMNS -1) { // Derniére case

      /* Affiche le char de fin en fonction du nombre de colonnes */
      if (nb_columns > 0) {
        lcd.write(6); // Char fin 1 / 1

      } else {
        lcd.write(5); // Char fin 0 / 1
      }

    } else { // Autres cases

      /* Affiche le char adéquat en fonction du nombre de colonnes */
      if (nb_columns >= 2) {
        lcd.write(4); // Char div 2 / 2
        nb_columns -= 2;

      } else if (nb_columns == 1) {
        lcd.write(3); // Char div 1 / 2
        nb_columns -= 1;

      } else {
        lcd.write(2); // Char div 0 / 2
      }
    }
  }
}

void write_lcd(void)
{
    char LCDbuff[20];

if (!setmode) {
    // Zeile 1
    lcd.setCursor(6, 0); 
    dtostrf(liter, 4, 0, LCDbuff);
    lcd.print(LCDbuff); lcd.print(" Liter");
    
    if ( LCD_Page == false ) {
              // Zeile 3
              lcd.setCursor(0, 2); 
              lcd.print("Uptime");

              // Zeile 4
              lcd.setCursor(0, 3);
              lcd.print(uptime);
    } else {
              // Zeile 3
              lcd.setCursor(0, 2); 
              lcd.print("Messwert Analog");

              // Zeile 4
              lcd.setCursor(0, 3);
              lcd.print(analogRead(analogPin));    
              
    }
 
 }
}

void loop()
{
    /*--------------------------------------------------------------
     remove delay (half second)
     --------------------------------------------------------------*/
  
    pin_stat = digitalRead(MODE_PIN);
    pinMillis = millis();
    if (pin_stat == LOW) {
        if (pinMillis - lastMillis > 200) {
            //USE_SERIAL.println("Impuls Low");
            lastMillis = pinMillis;
            mode_count = MODE_COUNT;
            if (!setmode) {
                setmode = true;
                old_mode = mode;
                //sprintf(buf, "Enter Settings-Mode %d (%s)", mode, modes[mode]);
                //USE_SERIAL.println(buf);
            } else {
                mode--;
                if (mode < 0) {
                    mode = 2;
                }
                //sprintf(buf, "Mode change to %d (%s)", mode, modes[mode]);
                //USE_SERIAL.println(buf);
            }
        }
    }

/*--------------------------------------------------------------  
 * remove delay (one second)
--------------------------------------------------------------*/
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
    if (currentMillis - startMillis >= sekunde) { // Hier eine Sekunde warten
        mqttclient.loop();
  startMillis = currentMillis;

    /******************************************************************************************
   *  1 Sekunden Takt 
   * *****************************************************************************************
   *  
   *  
   *******************************************************************************************/
    
    // Hier die Funktionen im Sekundentakt
    // ###################################    
      
        Uptime();
        ReadAnalog();
        
        if (setmode) {
            mode_count--;
            if (mode_count <= 0) {
                //sprintf(buf, "leaving Settings-Mode - change from %d to %d (%s)", old_mode, mode, modes[mode]);
                //USE_SERIAL.println(buf);
                old_mode = mode;
                setmode = false;
            }
        }
        if (!setmode) {
            if (mode == MODE_ZISTERNE) {
                new_valve = VALVE_ZISTERNE;
                reason = 0;
            } else if (mode == MODE_HAUSWASSER) {
                new_valve = VALVE_HAUSWASSER;
                reason = 0;
            } else if (mode == MODE_AUTO) {
                if (liter >= limit_high) {
                    new_valve = VALVE_ZISTERNE;
                    reason = 1;
                }
                if (liter <= limit_low) {
                    new_valve = VALVE_HAUSWASSER;
                    reason = 2;
        }
        
  
    }

        /*-----------------------------------------------------------
         * Wenn Ventil umzuschalten ist
        */
        if (new_valve != valve) {
            valve = new_valve;
            digitalWrite(VALVE_PIN, valve);
            //sprintf(buf, "Set Valve to %s (%s)", valves[valve], reasons[reason]);
            //USE_SERIAL.println(buf);
     }

        if (!setmode) {
            draw_progressbar(percent);

      }   
    }
write_lcd();      
 }
}
   /******************************************************************************************
   *  Ende Loop
   * ****************************************************************************************/
