/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
// This file contains static methods for API requests using Wifi / MQTT
#ifndef __ESP32_MQTT_H__
#define __ESP32_MQTT_H__

#include <Client.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>

#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h" // Update this file with your configuration
// !!REPLACEME!!
// The MQTT callback function for commands and configuration updates
// Place your message handler code here.

///////////////////////////////

// Initialize WiFi and MQTT for this board
Client *netClient;
CloudIoTCoreDevice *device;
CloudIoTCoreMqtt *mqtt;
MQTTClient *mqttClient;
unsigned long iat = 0;
String jwt;

///////////////////////////////
// Helpers specific to this board
///////////////////////////////
String getDefaultSensor()
{
  return "Wifi: " + String(WiFi.RSSI()) + "db";
}

String getJwt()
{
  iat = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iat, jwt_exp_secs);
  return jwt;
}

void setupWifi(char *ssid,char *password)
{
  Serial.println("Starting wifi a3pro");

  WiFi.mode(WIFI_STA);
  // WiFi.setSleep(false); // May help with disconnect? Seems to have been removed from WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }

  configTime(0, 0, ntp_primary, ntp_secondary);
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967)
  {
    delay(10);
  }
}

void connectWifi()
{
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
}

///////////////////////////////
// Orchestrates various methods from preceeding code.
///////////////////////////////
bool publishTelemetry(String data)
{
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char *data, int length)
{
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data)
{
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char *data, int length)
{
  return mqtt->publishTelemetry(subfolder, data, length);
}

void connect()
{
  connectWifi();
  mqtt->mqttConnect();
}

void setupCloudIoT(char *ssid,char *password){ 
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);

  setupWifi(ssid,password);
  netClient = new WiFiClientSecure();
  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, netClient, device);
  mqtt->setUseLts(true);
  mqtt->startMQTT();
}
WebServer server(80);
void handleRoot() {
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "<form method='post'>";
  html += "  <input type='text' name='ssid' placeholder='ssid'><br>";
  html += "  <input type='text' name='pass' placeholder='pass'><br>";
  html += "  <input type='submit'><br>";
  html += "</form>";
  server.send(200, "text/html", html);
}
void handleSubmit(){
  server.arg("ssid").toCharArray(config.ssid, 32);
	server.arg("pass").toCharArray(config.password, 32);
	EEPROM.write(0, 100);
	EEPROM.put<Config>(1, config);
	EEPROM.commit();		
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "ESSID:" + String(config.ssid) + "<br>";
  html += "key(pass):" + String(config.password) + "<br>";
  html += "<hr>";
  html += "<h1>now Reset!</h1>";
  server.send(200, "text/html", html);
  Serial.println("reset");
  ESP.restart();
}
void setupAp(){  
  IPAddress ip(192, 168, 10, 4);
  IPAddress subnet(255, 255, 255, 0);    
  DNSServer dnsServer;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(config.ssid,config.password);
  WiFi.softAPConfig(ip,ip,subnet); 
  server.on("/",HTTP_GET,handleRoot);
  server.on("/",HTTP_POST,handleSubmit);
  dnsServer.start(53, "*", ip);
  server.begin();
  Serial.println("wifi ap start!");
  while(1){
    dnsServer.processNextRequest();
    server.handleClient();
  }
}
void wifiSetup(){
  EEPROM.begin(1000);
	if (EEPROM.read(0) == 100) {
     EEPROM.get<Config>(1, config); 
     setupCloudIoT(config.ssid,config.password);   
  }else{
     setupAp();
  }
  delay(10); // <- fixes some issues with WiFi stability
}
#endif //__ESP32_MQTT_H__
