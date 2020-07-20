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

  Author:       Eisbaeeer, https://github.com/Eisbaeeer               
 
  Author:       Ethernet part: W.A. Smith, http://startingelectronics.com
                progress bar -  CC BY-SA 3.0 :  skywodd, https://www.carnetdumaker.net/articles/faire-une-barre-de-progression-avec-arduino-et-liquidcrystal/
				
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



// ##############################################################################################################################
// ---- HIER die Anpassungen vornehmen ----
// ##############################################################################################################################
// Hier die maximale Füllmenge des Behälters angeben. Dies gilt nur für symmetrische Behälter.
const float max_liter = 6300;                           

// Analoger Wert bei maximalem Füllstand (wird alle 30 Sekungen auf dem LCD angezeigt oder in der seriellen Konsole mit 9600 Baud.
const int analog_value = 763;                           

// Dichte der Flüssigkeit - Bei Heizöl bitte "1.086" eintragen, aber nur wenn die Kalibrierung mit Wasser erfolgt ist! 
// Bei Kalibrierung mit Wasser bitte "1.0" eintragen
const float dichte = 1.0;                              

// IP Adresse und Port des MQTT Servers
IPAddress mqttserver(192, 168, 1, 200);
const int mqttport = 1883;
// Wenn der MQTT Server eine Authentifizierung verlangt, bitte folgende Zeile aktivieren und Benutzer / Passwort eintragen
#define mqttauth
const char* mqttuser = "MQTT ID ";
const char* mqttpass = "MQTTPASS";                 

// IP Adresse, falls kein DHCP vorhanden ist. Diese Adresse wird nur verwendet, wenn der DHCP-Server nicht erreichbar ist.
IPAddress ip(192, 168, 1, 21);                          

// MAC-Addresse bitte anpassen! Sollte auf dem Netzwerkmodul stehen. Ansonsten eine generieren.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0A };    

// ##############################################################################################################################
// AB hier nichts mehr ändern!
// (Ausser ihr wisst, was ihr tut)
// ##############################################################################################################################

#define USE_SERIAL Serial

// No more delay
unsigned long startMillis;  // some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long periode = 1000;  // one seconds

byte one_minute = 60; // one minute
byte one_minute_count = 0;
byte one_hour = 60; // one hour
byte one_hour_count = 0;
byte one_day = 24; // one day
byte one_day_count = 0;

byte uptime_s = 0;
byte uptime_m = 0;
byte uptime_h = 0;
int uptime_d;

float percent;
float liter;
boolean LCD_Page;

// Analog IN
int analogPin = A0;
const int messungen = 60;     // Anzahl Messungen
int myArray[messungen];       // Array für Messwerte
float fuel = 0.0;             // Durchschnittswert
int pointer = 0;              // Pointer für Messung

// MQTT global vars
#include <PubSubClient.h>
unsigned int send_interval = 30; // the sending interval of indications to the server, by default 10 seconds
unsigned long last_time = 0; // the current time for the timer

boolean mqttReconnect = false;

// MQTT definitions
EthernetClient ethClient;
PubSubClient client(ethClient);
void callback(char * topic, byte * payload, unsigned int length);
// Declare subs
void Mqttpublish();



void defaultEthernet(void)
  {
    Ethernet.begin(mac, ip);  // initialize Ethernet device
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
  lcd.print("Version 1.4");
  lcd.setCursor(0, 3);
  lcd.print("github/Eisbaeeer");
  delay(2000);
  uptime_d = 0;

  setup_progressbar();

/*--------------------------------------------------------------  
 * Milliseconds start
--------------------------------------------------------------*/
  startMillis = millis();  //initial start time


// start MQTT client
  client.setServer(mqttserver, mqttport);

  Mqttpublish();

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
 
    USE_SERIAL.print(F("IP: "));
    USE_SERIAL.println(Ethernet.localIP());       
}




void loop()
{
 
/*--------------------------------------------------------------  
 * check ehternet services
--------------------------------------------------------------*/
    switch (Ethernet.maintain()) {
      case 1:
       //renewed fail
       USE_SERIAL.println(F("Error: renewed fail"));
       break;

      case 2:
        //renewed success
       USE_SERIAL.println(F("Renewed success"));
       //print your local IP address:
       USE_SERIAL.print(F("My IP address: "));
       USE_SERIAL.println(Ethernet.localIP());
       break;

      case 3:
       //rebind fail
       USE_SERIAL.println(F("Error: rebind fail"));
       break;

      case 4:
       //rebind success
       USE_SERIAL.println(F("Rebind success"));
       //print your local IP address:
       USE_SERIAL.print(F("My IP address: "));
       USE_SERIAL.println(Ethernet.localIP());
       break;

      default:
       //nothing happened
       break;
  }
 
 
  
/*--------------------------------------------------------------  
 * remove delay (one second)
--------------------------------------------------------------*/
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentMillis - startMillis >= periode)  // Hier eine Sekunde (periode)
  { 
  startMillis = currentMillis;

    /******************************************************************************************
   *  1 Sekunden Takt 
   * *****************************************************************************************
   *  
   *  
   *******************************************************************************************/
    
    // Hier die Funktionen im Sekundentakt
    // ###################################    
                 
     uptime_s++;
     if (uptime_s >59)
        { uptime_s = 0; 
          uptime_m ++;
        }
     if (uptime_m >59)
        { uptime_m = 0; 
          uptime_h ++;
        }
     if (uptime_h >23)
        { uptime_h = 0; 
          uptime_d ++;
        }
        

    // read the analog value and build floating middle
    myArray[pointer] = analogRead(analogPin);      // read the input pin
    
    if ( pointer > messungen ) {
      pointer = 0;
    } else {
      pointer++;  
    }

    // Werte aufaddieren
    for (int i = 0; i < messungen; i++)
      {
       fuel = fuel + myArray[i];
      }
    // Summe durch Anzahl
       fuel = fuel / messungen;

    percent = fuel * 0.132;
      if (percent > 100) {
         percent = 100;
      }
    float calc = max_liter / analog_value;    // calculate percent
          calc = calc * dichte;               // calculate dichte
    liter = fuel * calc;                      // calculate liter
        if (liter > max_liter) {
           liter = max_liter;
        }
    USE_SERIAL.print(F("Analog: "));
    USE_SERIAL.println(fuel);
    USE_SERIAL.print(F("Prozent: "));
    USE_SERIAL.println(percent);
    USE_SERIAL.print(F("Liter: "));
    USE_SERIAL.println(liter);

    // print out to LCD
    draw_progressbar(percent);
    write_lcd();
         
   /******************************************************************************************
   *  30 Sekunden Takt 
   * *****************************************************************************************
   *  
   *  
   *******************************************************************************************/
   if ((millis()) >= (last_time + send_interval * 1000)) {
        last_time = millis(); 

        LCD_Page = !LCD_Page;
        MqttSub();
      }
  
  /******************************************************************************************
   *  1 Minuten Takt 
   * *****************************************************************************************
   *  
   *  
   *******************************************************************************************/
    if (one_minute_count <= one_minute) {
        one_minute_count++;
        } else {
        one_minute_count = 0; // reset counter
        one_hour_count++;
                
        USE_SERIAL.print(F("Uptime: "));
        USE_SERIAL.print(uptime_m);
        USE_SERIAL.println(F(" Minuten"));

        // MQTT reconnect timeout
        //Mqttpublish();
        mqttReconnect = false;     
        
     }

   /******************************************************************************************
   *  1 Stunden Takt 
   * *****************************************************************************************
   *  
   *  
   *******************************************************************************************/
     if (one_hour_count == one_hour) {
        one_hour_count = 0; // reset counter
                
    // Hier die Funktionen im Stundentakt
    // ###################################

      USE_SERIAL.println(F("ONE HOUR"));
      } 
      
  }

  
}
   /******************************************************************************************
   *  Ende Loop
   * ****************************************************************************************
   *  Beginn Unterprogramme
   ******************************************************************************************/


void MqttSub(void)
{
  // MQTT stuff
  // If the MQTT connection inactively, then we try to set it and to publish/subscribe
  if ( mqttReconnect == false )
  {
    if (!client.connected()) {
        USE_SERIAL.print(F("Conn MQTT"));
         // Connect and publish / subscribe
        #ifdef mqttauth
         if (client.connect("Zisterne", mqttuser, mqttpass)) {
        #else
         if (client.connect("Zisterne")) {
        #endif
            USE_SERIAL.println(F("succ"));
            // pulish values
            Mqttpublish();
          } else {
            // If weren't connected, we wait for 10 seconds and try again
            USE_SERIAL.print(F("Failed, rc="));
            USE_SERIAL.print(client.state());
            USE_SERIAL.println(F(" try again every min"));
            mqttReconnect = true;
         }
          // If connection is active, then sends the data to the server with the specified time interval 
          } else {
            Mqttpublish(); 
          }
    }
}

void write_lcd(void)
{
    char LCDbuff[20];
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
              String uptimesum;
              uptimesum = String(uptime_d);
              uptimesum = String(uptimesum + "d ");
              uptimesum = String(uptimesum + uptime_h);
              uptimesum = String(uptimesum + "h ");
              uptimesum = String(uptimesum + uptime_m);
              uptimesum = String(uptimesum + "m");
              lcd.print(uptimesum);    
    } else {
              // Zeile 3
              lcd.setCursor(0, 2); 
              lcd.print("Messwert Analog");

              // Zeile 4
              lcd.setCursor(0, 3);
              lcd.print(analogRead(analogPin));    
              
    }
 
 }

void Mqttpublish(void)
{  
      char buff[25];
      dtostrf(fuel, 5, 2, buff);
      client.publish("Zisterne/Analog", buff );

      dtostrf(liter, 5, 0, buff);
      client.publish("Zisterne/Liter", buff );

      dtostrf(percent, 5, 0, buff);
      client.publish("Zisterne/Prozent", buff );
    
      // System    
      String upsum;
      upsum = String(uptime_d);
      upsum = String(upsum + "d ");
      upsum = String(upsum + uptime_h);
      upsum = String(upsum + "h ");
      upsum = String(upsum + uptime_m);
      upsum = String(upsum + "m");
      upsum.toCharArray(buff, 25);
      client.publish("Zisterne/Uptime", buff); 
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
