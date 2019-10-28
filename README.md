![Logo](Pics/4x16_LCD.png)
# Arduino.Ethernet.Zisterne

## Beschreibung
Dieses Projekt ermöglicht eine Füllstandsmessung per Pegelsonde.
Zur Verwendung kommt ein Arduino Nano, ein W5100 Ethernet Shield, 
ein 4-20mA Stromwandler und ein 4x16 Zeichen LCD mit I2C Schnittstelle.

Die Meßdaten werden per MQTT an einen Server übertragen und auf dem Display angezeigt.

In der ino Datei müssen noch entsprechende Anpassungen gemacht werden.
Diese sind am Anfang des Codes.

- Volumen der Zisterne    
- Dichte des Mediums   
- Analog Wert der Kalibrierung
- IP-Adresse des MQTT Servers   
- Port des MQTT Servers   
- MQTT Benutzername   
- MQTT Passwort   
- IP-Adresse des Nano (falls kein DHCP zur Verfügung steht)   
- MAC-Adresse des Nano   

## Verschaltung der Module
Ich verwende Standard Module, welche über das Internet verfügbar sind. Die Klemmblöcke habe ich ausgelötet und durch Stifte erstetzt.
Als Basis verwende ich eine Lochraster Platine. Damit sind alle Baugruppen schnell ersetzbar.
Grundsätzlich ist im Sketch schon beschrieben, welche Ports verwendung finden. Ich habe als Hilfestellung meine Verdrahtung hinzugefügt.
![Logo](Pics/Schaltplan.jpg)

## Abgleich
Bitte erst ganz durchlesen!
1. Aufstecken der ersten Baugruppe (Step-up Wandler, Modul ganz unten) und anlegen der 5V Eingangsspannung. Mit Multimeter an VOUT+ und VOUT- messen und mit dem Poti maximal 24V einstellen.   
2. Spannung an den Schraubklemmen Drucksensor messen. Max. 24V (je nach Sonde. Bitte Datenblatt der Sonde lesen)   
3. Wandlermodul aufstecken (oberes Modul unterhalb der Platine mit den 2 blauen Potis) und die Jumper J1 und J2 abziehen.   
4. Pegelsonde anschließen   
5. Linker Poti stellt Nullpunkt ein. Dazu mit Multimeter an hellblau (mittlerer Pin vom Wandlermodul) auf 0V einstellen. Es kann auch ein negativer Wert angezeigt werden. Das Poti macht locker 20 Umdrehungen, bis es von min auf max geht.   
6. Rechter Poti stellt dann den Range ein. Minimum ist bei Tank leer, maximum bei Tank voll (maximum darf 3.3V nicht übersteigen!   



## Changelog

### 1.3
- (20191028 Eisbaeeer)   
- MQTT Port ist jetzt konfigurierbar
- MQTT Authentifizierung 

### 1.2
- (20191019 Eisbaeeer)   
- LCD wechselt jetzt alle 30 Sek. zwischen Uptime und Analog Messwert   
- (20191024 Eisbaeeer)
- Fixed update_d in MQTT

### 1.1
- (20191019 Eisbaeeer)   
- Dichte Berechnung hinzugefügt   

### 1.0
- (Eisbaeeer)
Initial Version

## License
The MIT License (MIT)
Copyright (c) 2019 Eisbaeeer <eisbaeeer@gmail.com> 
