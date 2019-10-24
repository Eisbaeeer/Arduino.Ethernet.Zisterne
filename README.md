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
- IP-Adresse des Nano (falls kein DHCP zur Verfügung steht)   
- MAC-Adresse des Nano   

## Changelog

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
