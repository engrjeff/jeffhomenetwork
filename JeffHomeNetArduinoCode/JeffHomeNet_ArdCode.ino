#include<SoftwareSerial.h>
#include<DHT.h>
#include<LiquidCrystal_I2C.h>
#include<ArduinoJson.h>

#define TX 2
#define RX 3

#define PHSENSOR A6
#define PUMP 4
#define VALVE 5
#define FLOATSWITCH 7
#define DHTPIN 6
#define DHTTYPE DHT22

SoftwareSerial ard(RX, TX);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
const int objCapacity = JSON_OBJECT_SIZE(6);
StaticJsonDocument<objCapacity> sensorObj;

byte deg[8] = {
  B01100,
  B10010,
  B10010,
  B01100,
  B00000,
  B00000,
  B00000,
  B00000,
};

void initialize() {
  //Comms
  ard.begin(9600);
  Serial.begin(9600);
  //LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, deg);
  //PINS
  pinMode(PUMP, OUTPUT);
  pinMode(VALVE, OUTPUT);
  pinMode(FLOATSWITCH, INPUT_PULLUP);
  pinMode(PHSENSOR, INPUT);
  //DHT
  dht.begin();

  //States
  digitalWrite(VALVE, LOW);
  digitalWrite(PUMP, LOW);

  showStartMsg();
}

void showStartMsg() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TeePee App");
  delay(500);
  lcd.setCursor(0, 1);
  lcd.print("Initializing....");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reading data...");
}

void GetSensorReadings() {
  float tempVal = GetTemperature();
  float humVal = GetHumidity();
  float phVal = GetPhValue();
  bool valveState = GetBinaryState(VALVE);
  bool pumpState = GetBinaryState(PUMP);
  bool floatState = GetBinaryState(FLOATSWITCH) == HIGH ? LOW : HIGH;

  //  Serial.println(tempVal);
  //  Serial.println(humVal);
  //  Serial.println(phVal);
  //  Serial.println(valveState);
  //  Serial.println(pumpState);
  //  Serial.println(floatState);

  //Convert to JSON String
  sensorObj["TempValue"] = tempVal;
  sensorObj["HumValue"] = humVal;
  sensorObj["PhValue"] = phVal;
  sensorObj["ValveState"] = valveState;
  sensorObj["PumpState"] = pumpState;
  sensorObj["FloatState"] = floatState;

  String json = "";
  serializeJson(sensorObj, json);
  Serial.println(json);
  serializeJson(sensorObj, ard);
  printSensorDataToLCD(tempVal, humVal, phVal, valveState, pumpState, floatState);

  delay(1000);
}

String trimFloat(float f){
  String fStr = String(f);
  return fStr.substring(0, fStr.length() - 1);
}
void printSensorDataToLCD(float t, float h, float ph, bool v, bool p, bool f) {
  String T = "T: " + trimFloat(t);
  String H = "H: " + trimFloat(h) + "%";
  String pH = "pH:" + trimFloat(ph);

  //temp
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(T);
  lcd.setCursor(T.length(), 0);
  lcd.write(0);
  lcd.setCursor(T.length() + 1, 0);
  lcd.print("C");

  //hum
  lcd.setCursor(0, 1);
  lcd.print(H);

  //pH
  lcd.setCursor(H.length() + 1, 1);
  lcd.print(pH);


}
bool GetBinaryState(int pin) {
  return digitalRead(pin);
}
float GetPhValue() {
  //For Potentiometer simulation only
  float potReading = analogRead(PHSENSOR);
  constrain(potReading, 10, 1014);
  float phLevel = map(potReading, 10, 1014, 0, 14);
  return phLevel;
}
float GetTemperature() {
  float t = dht.readTemperature();

  if (isnan(t)) {
    return 0.0;
  }
  else {
    return t;
  }
}

float GetHumidity() {
  float h = dht.readHumidity();

  if (isnan(h)) {
    return 0.0;
  }
  else {
    return h;
  }
}

void setup() {
  initialize();
}

void loop() {
  GetSensorReadings();
  delay(1000);
}
void test() {
  while (ard.available()) {
    String data = ard.readString();
    Serial.print("Received: ");
    Serial.println(data);
    ard.write("hi esp!");
  }
  //delay(1000);
}
