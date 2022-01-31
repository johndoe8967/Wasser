# Wasserwächter

Kleine Leckagen im Rohrleitungssystem eines Hauses können neben dem erhöhten Wasserverbrauch auch große Schäden 
an der Gebäudesubstanz und den Einrichtungsgegenständen anrichten. 

Ziel dieses Projektes ist es Leckagen zu entdecken und zu melden bevor es zu nennenswerten Schäden kommt. 
Dazu ist es notwendig den beabsichtigten Gebrauch von Wasser vom unbeabsichtigten Auslaufen zu unterscheiden.

Als erstes muss der Wasserverbrauch detektiert werden, 
dazu ist es zweckmäßig den bereits vorhandenen Wasserzähler heranzuziehen. 
Typischerweise haben diese ein präzises Zählwerk, welches jedoch nur optisch abgetastet werden kann.

Die Schwierigkeit besteht nun darin geeignete Algorithmen zu finden die zuverlässig erkennen aber keine Fehlalarme produzieren

## Messung
Digitaleingang: über Interrupt wird jeder Impuls des Zählwerkes zeitgenau vermessen und gezählt
Analogeingang: über einen Analogeingang können bei mangelhafter Signalaufbereitung auch digitale Nachbearbeitungen durchgeführt werden (noch keine Implementierung)

## Datenübertragung
Über MQTT werden die relevanten Messdaten zyklisch (z.B. alle 5s) in die Cloud übertragen. 
Das darin enthaltene JSON Paket ist wie folgt aufgebaut.

{"name":"<DEVICENAME>","field":"Water","ChangeCounter":<value>,"timeSinceChange":<Dauer>,"lastDuration":<Dauer>,"flowRate":<value>,"time":<timestamp>,}

## Datenspeicherung
In der Cloud wird das MQTT Datenpaket von Telegram ausgewertet und in eine InfluxDB gespeichert. 

## Auswertung
Die Auswertung erfolgt über Grafana