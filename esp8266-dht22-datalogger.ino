#include "FS.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "DHT.h"
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>

#define LED_PIN 4
#define DHTPIN 14
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define BLINK_AMT 10
#define BLINK_DELAY 100

/****************** DHT22 ****************************************************/

DHT dht(DHTPIN, DHTTYPE);
float lastF = 0;
float lastH = 0;

boolean successfulRead = false;

/****************** WiFi *****************************************************/

const char* ssid = "your wifi";
const char* password = "your pass";

ESP8266WebServer server(80);

/****************** TIMINGS **************************************************/

unsigned long currentMillis;
unsigned long previousDHTMillis = 0;
unsigned long dhtInterval = 2000;

unsigned long previousTCMillis = 0;
unsigned long tcUpdateInterval = 30000;

/*********************** HTTP ****************************************/

#define HTTP_SERVER_ROOT    "http://your-site"
#define HTTP_ADD_ENTRY      "addDataEntry.php"
#define HTTP_ADD_ERROR      "addError.php"
#define HTTP_KEY            "secret"
#define DEVICE_ID           "an id"
#define HTTP_USERNAME       "user"
#define HTTP_PASSWORD       "password"

//Replace with building URLs in your own format
const char THING_TEMP_URL[] PROGMEM = HTTP_SERVER_ROOT HTTP_ADD_ENTRY "?key=" HTTP_KEY "&newEntry=Y&DEVICE_ID=" DEVICE_ID "&data_value=";
const char THING_HUMID_URL[] PROGMEM = HTTP_SERVER_ROOT HTTP_ADD_ENTRY "?key=" HTTP_KEY "&newEntry=Y&DEVICE_ID=" DEVICE_ID "&data_value=";
const char THING_ERROR_URL[] PROGMEM = HTTP_SERVER_ROOT HTTP_ADD_ERROR "?key=" HTTP_KEY "&DEVICE_ID=" DEVICE_ID;

bool errorSent = false;
int errorCount = 0;

/****************** FUNCTIONS ************************************************/

void wsSendTemp();
void handleNotFound();
void readAndPrintDHT();
void blinkLedError();
void updateTC();
void updateTCTemp();
void updateTCHumidity();
void sendTCError();
bool shouldSendError();

void wsSendTemp() {
  digitalWrite(LED_PIN, 1);
  
  float heatIndex = dht.computeHeatIndex(lastF, lastH);
  
  String message = F("Temp: ");
  message += lastF;
  message += F(" *F \nHumidity: ");
  message += lastH;
  message += "%\nHeat index: ";
  message += heatIndex;
  message += " *F";
  
  server.send(200, "text/plain", message);
  
  digitalWrite(LED_PIN, 0);
}

void handleNotFound(){
  digitalWrite(LED_PIN, 1);
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
  digitalWrite(LED_PIN, 0);
}

void readAndPrintDHT() {
  digitalWrite(LED_PIN, 1);
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  lastF = f;
  lastH = h;

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    
    successfulRead = false;
    return;
  }
  
  successfulRead = true;
  
  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.print(hif);
  Serial.println(" *F");
  
  digitalWrite(LED_PIN, 0);
}

void blinkLedError() {
  for (int i = 0; i < BLINK_AMT; i++) {
    digitalWrite(LED_PIN, 1);
    delay(BLINK_DELAY);
    digitalWrite(LED_PIN, 0);
    delay(BLINK_DELAY);
  }
}

void updateTC() {
  if (successfulRead) {
    digitalWrite(LED_PIN, 1);
    Serial.println(F("Updating over Http"));
    
    updateTCTemp();
    updateTCHumidity();
    
    digitalWrite(LED_PIN, 0);
    
    errorSent = false;
  } else {
    Serial.println(F("Skipping Http Update"));
    Serial.print("Error Count ");
    Serial.println(errorCount);
    if (!errorSent && shouldSendError()) {
      sendTCError();
      errorSent = true;
    }
  }
}

void updateTCTemp() {
  String url = ""; //build temp log url
  url += THING_TEMP_URL;
  url += lastF;
  Serial.print(F("Connecting to: "));
  Serial.println(url);
  
  HTTPClient http;
  http.begin(url);
  http.setAuthorization(HTTP_USERNAME, HTTP_PASSWORD);
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println(F("Error updating Http server"));
    
    blinkLedError();
  } else {
    Serial.println(F("Http Update Successful"));
  }
  
  http.end();
}

void updateTCHumidity() {
  String url = ""; //build humidity log url
  url += THING_HUMID_URL;
  url += lastH;
  Serial.print(F("Connecting to: "));
  Serial.println(url);
  
  HTTPClient http;
  http.begin(url);
  http.setAuthorization(HTTP_USERNAME, HTTP_PASSWORD);
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println(F("Error updating Http"));
    
    blinkLedError();
  } else {
    Serial.println(F("Http Update Successful"));
  }
  
  http.end();
}

bool shouldSendError() {
  if (errorCount > 10) {
    errorCount = 0;
    
    return true;
  } else {
    errorCount++;
    
    return false;
  }
}

void sendTCError() {
  String url = ""; //build error url
  url += THING_ERROR_URL;
  url += "&error_name=DataReadError";
  url += "&error_desc=Error%20Reading%20DHT22%20Sensor";
  Serial.print(F("Connecting to: "));
  Serial.println(url);
  
  HTTPClient http;
  http.begin(url);
  http.setAuthorization(HTTP_USERNAME, HTTP_PASSWORD);
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println(F("Error sending error to Http Server"));
    
    blinkLedError();
  } else {
    Serial.println(F("Http Server error post Successful"));
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("WifiThermometer");
  Serial.println("esp8266-dht22-datalogger");
  Serial.println("https://github.com/mikelduke/esp8266-dht22-datalogger");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, 0);
  delay(100);
  digitalWrite(LED_PIN, 1);
  delay(100);
  digitalWrite(LED_PIN, 0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, 0);
    delay(10);
    digitalWrite(LED_PIN, 1);
    delay(40);
    Serial.print(".");
  }

  Serial.println("Wifi Connected!");

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  //HTTP Server
  server.on("/", wsSendTemp);
  server.on("/index.html", wsSendTemp);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  //DHT
  dht.begin();
}

void loop() {
  currentMillis = millis();
  
  //DHT22 Checking
  if (currentMillis - previousDHTMillis >= dhtInterval) {
    previousDHTMillis = currentMillis;
    
    readAndPrintDHT();
    
    if (!successfulRead) {
      blinkLedError();
    }

    Serial.print(F("Wifi Power: "));
    Serial.println(WiFi.RSSI());
  }
  
  yield();
  
  //Update Http
  if (currentMillis - previousTCMillis >= tcUpdateInterval) {
    previousTCMillis = currentMillis;
    updateTC();
  }

  yield();
  
  server.handleClient();
}

