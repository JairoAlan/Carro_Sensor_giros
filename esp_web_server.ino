#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Arduino_JSON.h>
#include "LittleFS.h"

// Conexion a la red
const char* ssid = "JACFO_3509";
const char* password = "473m25V+";

// Crea un AsyncWebServer object en el puerto 80
AsyncWebServer server(80);

// Crea un Event Source en /events
AsyncEventSource events("/events");

// Variable Json que obtiene las lecturas del sensor
JSONVar readings;

// Variables
unsigned long lastTime = 0;  
unsigned long lastTimeTemperature = 0;
unsigned long lastTimeAcc = 0;
unsigned long gyroDelay = 10;
unsigned long temperatureDelay = 1000;
unsigned long accelerometerDelay = 200;

// Crea un objeto del sensor 
Adafruit_MPU6050 mpu;

sensors_event_t a, g, temp;

float gyroX, gyroY, gyroZ;
float accX, accY, accZ;
float temperature;

//Giroscopio desviacion (error) se puede ajustar a lo que se necesite
float gyroXerror = 0.07;
float gyroYerror = 0.03;
float gyroZerror = 0.01;

void initWiFi() {
  Serial.println("Iniciando WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando al WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    Serial.print(".");
    delay(1000);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("Conectado al WiFi");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Fallo al conectar al Wifi despues de 20 intentos");
  }
}

void initLittleFS() {
  Serial.println("Iniciando LittleFs...");
  if (!LittleFS.begin()) {
    Serial.println("Un error Ocurrio muentra se montaba LittleFS");
  } else {
    Serial.println("LittleFS se monto exitosamente");
  }
}

void initMPU(){
  Serial.println("Iniciando MPU6050...");
  if (!mpu.begin()) {
    Serial.println("Se fallo para encontrarel chip MPU6050");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Encontrado!");
}

String getGyroReadings(){
  mpu.getEvent(&a, &g, &temp);
  Serial.println("Obteniendo las lecturas del giroscopio...");
  
  float gyroX_temp = g.gyro.x;
  if(abs(gyroX_temp) > gyroXerror)  {
    gyroX += gyroX_temp / 50.00;
  }
  
  float gyroY_temp = g.gyro.y;
  if(abs(gyroY_temp) > gyroYerror) {
    gyroY += gyroY_temp / 70.00;
  }

  float gyroZ_temp = g.gyro.z;
  if(abs(gyroZ_temp) > gyroZerror) {
    gyroZ += gyroZ_temp / 90.00;
  }

  readings["gyroX"] = String(gyroX);
  readings["gyroY"] = String(gyroY);
  readings["gyroZ"] = String(gyroZ);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

String getAccReadings() {
  mpu.getEvent(&a, &g, &temp);
  Serial.println("Obteniendo las lecturas del acelerometro...");
  
  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;
  
  readings["accX"] = String(accX);
  readings["accY"] = String(accY);
  readings["accZ"] = String(accZ);
  
  String accString = JSON.stringify(readings);
  return accString;
}

String getTemperature(){
  mpu.getEvent(&a, &g, &temp);
  Serial.println("Obteniendo lecturas de la temperatura...");
  
  temperature = temp.temperature;
  return String(temperature);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");
  initWiFi();
  initLittleFS();
  initMPU();
  Serial.println("Setup complete");

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Resetting gyroscope values...");
    gyroX = 0;
    gyroY = 0;
    gyroZ = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetX", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Resetting gyroX...");
    gyroX = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetY", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Resetting gyroY...");
    gyroY = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetZ", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Resetting gyroZ...");
    gyroZ = 0;
    request->send(200, "text/plain", "OK");
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  if ((millis() - lastTime) > gyroDelay) {
    Serial.println("Sending gyro readings...");
    events.send(getGyroReadings().c_str(), "gyro_readings", millis());
    lastTime = millis();
  }
  if ((millis() - lastTimeAcc) > accelerometerDelay) {
    Serial.println("Sending accelerometer readings...");
    events.send(getAccReadings().c_str(), "accelerometer_readings", millis());
    lastTimeAcc = millis();
  }
  if ((millis() - lastTimeTemperature) > temperatureDelay) {
    Serial.println("Sending temperature readings...");
    events.send(getTemperature().c_str(), "temperature_reading", millis());
    lastTimeTemperature = millis();
  }
}
