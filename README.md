# esp8266-dht22-datalogger
Reads temperature and humidity from a DHT22 sensor and logs output to an http server.

The program will log to an error url if there is an error reading the sensor. The http server 
could log these to a database or send an email to someone.

## Configuration
You will need to replace the wifi info with yours and change how the urls are built to work with whatever server is being used.

