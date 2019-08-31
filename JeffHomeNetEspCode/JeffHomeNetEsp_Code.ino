#include<SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

const char* SSID = "";
const char* PASS = "";

const char* apSSID = "JeffHomeNet";
const char* apPass = "jeffsegovia0815";

#define tx 5 //GPIO5 --> D1`
#define rx 4 //GPIO4 --> D2

#define CONNECTED_LED 14 //--> GPIO14 : D5 (Yellow)
#define DISCONNECTED_LED 12 //--> GPIO12 : D6 (Red)

const unsigned long connectDelay = 500;
const unsigned long connectTimeout = 15000;

bool isConnected = true;
String WiFiConfigJson = "";

ESP8266WebServer server(80);
SoftwareSerial espSerial(rx, tx);

bool saveConfig(String nssid, String npass) {
  StaticJsonDocument<512> doc;
  doc["ssid"] = nssid;
  doc["password"] = npass;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing."));
    configFile.close();
    return false;
  }

  serializeJson(doc, configFile);
  serializeJson(doc, Serial);
  configFile.close();
  return true;
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println(F("Failed to open JSON file."));
    configFile.close();
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println(F("Config file size is too large."));
    configFile.close();
    return false;
  }

  //Buffer
  std::unique_ptr<char[]> buf(new char[size]);
  //Read
  configFile.readBytes(buf.get(), size);
  //Parse JSON
  StaticJsonDocument<200> doc;
  auto  err = deserializeJson(doc, buf.get());
  if (err) {
    Serial.println(F("Failed to parse config file."));
    configFile.close();
    return false;
  }
  String json = "";
  serializeJson(doc, json);
  Serial.println("Loaded:");
  serializeJson(doc, Serial);

  WiFiConfigJson = json;
  Serial.println(WiFiConfigJson); 
  configFile.close();

  return true;
}


void wifiConnect() {
  //Start @ STA Mode
  isConnected = true;
  if (!loadConfig())return;

  const size_t cap = JSON_OBJECT_SIZE(2) + 50;
  DynamicJsonDocument doc(cap);

  deserializeJson(doc, WiFiConfigJson);

  Serial.print("WiFiConfigJson : " + WiFiConfigJson);

  const char* ssid = doc["ssid"];
  const char* pass = doc["password"];
  Serial.println(ssid);
  Serial.println(pass);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.softAPdisconnect();
  WiFi.begin(ssid, pass);

  //wait for connection
  //save start time of connection attempt
  int startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(connectDelay);
    digitalWrite(DISCONNECTED_LED, !digitalRead(DISCONNECTED_LED));
    unsigned long timeElapsed = millis() - startTime;
    if (timeElapsed > connectTimeout)
    {
      Serial.println(F("Cannot connect...."));
      Serial.println(F("Switching to AP Mode..."));
      digitalWrite(DISCONNECTED_LED, HIGH);
      isConnected = false;
      break;
    }
  }

  if (!isConnected) {
    Serial.println(F("Configuring Access Point..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPass);
    IPAddress IP = WiFi.softAPIP();
    Serial.print(F("Access Point IP: "));
    Serial.println(IP);
    WiFi.printDiag(Serial);
  }

  else {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(DISCONNECTED_LED, LOW);
    digitalWrite(CONNECTED_LED, HIGH);
  }
}

void initializeRoutes() {
  server.on("/", handleRoot);
  server.on("/getwifiparams", HTTP_POST, handleWiFiParams);
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  server.begin();
  Serial.println("HTTP server started");
}

void handleWiFiParams() {
  String params = server.arg("plain");
  Serial.println(params);
  //parse JSON data
  const size_t cap = JSON_OBJECT_SIZE(2) + 96;
  DynamicJsonDocument doc(cap);
  DeserializationError err = deserializeJson(doc, params);
  if (err) return;
  SSID = doc["ssid"];
  PASS = doc["password"];
  Serial.print("New SSID: ");
  Serial.println(SSID);
  Serial.print("New Password: ");
  Serial.println(PASS);

  String nssid = doc["ssid"];
  String npass = doc["password"];

  if (!saveConfig(nssid, npass)) return;

  server.send(200, "text/plain", "OK");
  
  delay(connectDelay);
  wifiConnect();


}
void handleRoot()
{
  String rootPath = "";
  rootPath = isConnected ? "/index.html" : "/config.html";
  handleFileRead(rootPath);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void)
{
  pinMode(CONNECTED_LED, OUTPUT);
  pinMode(DISCONNECTED_LED, OUTPUT);
  espSerial.begin(9600);
  Serial.begin(9600);
  SPIFFS.begin();
  //saveConfig("SegoviaWiFix", "SegoviaFamilyx"); //test
  wifiConnect();
  initializeRoutes();
}

void loop(void)
{
  server.handleClient();
  handleSerial();
}

void handleSerial() {
  while (espSerial.available()) {
    String data = espSerial.readString();
    Serial.print("Received: ");
    Serial.println(data);
    espSerial.write("hello ard!");
  }
}

//SPIFFS
String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  } else if (filename.endsWith(".svg")) {
    return "image/svg+xml";
  }

  return "text/plain";
}

bool handleFileRead(String path) {

  if (path.endsWith("/")) {
    path += "index.html";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}
