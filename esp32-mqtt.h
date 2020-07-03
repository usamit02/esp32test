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

#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h"
Client *netClient;
CloudIoTCoreDevice *device;
CloudIoTCoreMqtt *mqtt;
MQTTClient *mqttClient;
unsigned long iat = 0;
String jwt;
String getDefaultSensor(){
  return "Wifi: " + String(WiFi.RSSI()) + "db";
}
String getJwt(){
  iat = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iat, jwt_exp_secs);
  return jwt;
}
bool publishTelemetry(String data){
  return mqtt->publishTelemetry(data);
}
bool publishTelemetry(const char *data, int length){
  return mqtt->publishTelemetry(data, length);
}
bool publishTelemetry(String subfolder, String data){
  return mqtt->publishTelemetry(subfolder, data);
}
bool publishTelemetry(String subfolder, const char *data, int length){
  return mqtt->publishTelemetry(subfolder, data, length);
}
void setupWifi(char *ssid="",char *pass=""){ 
  wifiStatus=0;  
  Serial.println("WiFi starting");
  WiFi.mode(WIFI_STA);
  delay(10);
  if(sizeof(ssid)>0){
    WiFi.begin(ssid, pass);
  }else{
    WiFi.begin();
  }  
  Serial.println("Connecting to WiFi");  
}
void setupCloudIoT(char *ssid,char *password){ 
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);
  netClient = new WiFiClientSecure();
  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, netClient, device);
  mqtt->setUseLts(true);
  mqtt->startMQTT();
}
WebServer server(80);   
DNSServer dnsServer;
void handleRoot() {
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "<form method='post'>";
  html += "  <input type='text' name='ssid1' placeholder='ssid'><br>";
  html += "  <input type='text' name='pass1' placeholder='pass'><br><br>";
  html += "  <input type='text' name='ssid2' placeholder='ssid'><br>";
  html += "  <input type='text' name='pass2' placeholder='pass'><br>";
  html += "  <input type='submit'><br>";
  html += "</form>";
  server.send(200, "text/html", html);
}
void handleSubmit(){
  server.arg("ssid1").toCharArray(config.ssid[0], 32);
	server.arg("pass1").toCharArray(config.pass[0], 32);
  server.arg("ssid2").toCharArray(config.ssid[1], 32);
	server.arg("pass2").toCharArray(config.pass[1], 32);
	EEPROM.write(0, 100);
	EEPROM.put<Config>(1, config);
	EEPROM.commit();		
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "ESSID:" + String(config.ssid[0]) + "<br>";
  html += "key(pass):" + String(config.pass[0]) + "<br>";
  html += "<hr>";
  html += "ESSID:" + String(config.ssid[1]) + "<br>";
  html += "key(pass):" + String(config.pass[1]) + "<br>";
  html += "<hr>";
  html += "<h1>now Reset!</h1>";
  server.send(200, "text/html", html);
  Serial.println("reset");
  ESP.restart();
}
void setupAp(){   
  wifiStatus=10;
  IPAddress ip(192, 168, 10, 4);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp32","");
  delay(100);
  WiFi.softAPConfig(ip,ip,subnet); 
  server.on("/",HTTP_GET,handleRoot);
  server.on("/",HTTP_POST,handleSubmit);
  dnsServer.start(53, "*", ip);
  delay(10);
  server.begin();
  Serial.println("wifi ap start!");
}
void wifiSetup(){
  EEPROM.begin(1000);
	if (EEPROM.read(0) == 100) {
     EEPROM.get<Config>(1, config); 
     setupWifi();
  }else{
     setupAp();
  }
  delay(10); // <- fixes some issues with WiFi stability
}
void wifiLost(){
  wifiSeconds=0;
  if(wifiRetry<3){      
    setupWifi(config.ssid[wifiRetry%2],config.pass[wifiRetry%2]);
    wifiRetry++;    
  }else{
    setupAp();
  }
}
void wifiLoop(){
  if(wifiStatus==1){//mqtt connected
    mqtt->loop();
  }else if(wifiStatus==10){//wifi AP seaching
    dnsServer.processNextRequest();
    server.handleClient();
  }
}
void wifiCheck(){
  if(wifiStatus==0){//wifi STA searching
    if (WiFi.status() == WL_CONNECTED){
      wifiStatus=111;
      configTime(0, 0, ntp_primary, ntp_secondary);
      Serial.println("Waiting on time sync...");
      while (time(nullptr) < 1510644967){
        delay(10);
      }
      Serial.println("sync!");
      setupCloudIoT(config.ssid[0],config.pass[0]);                    
      wifiStatus=1;wifiSeconds=0;
    }else if(wifiSeconds>60){
      wifiLost();
    }else{
      wifiSeconds++;Serial.print(".");
    }  
  }else if(wifiStatus==1&&WiFi.status() != WL_CONNECTED){//&&!mqttClient->connected()){//mqtt lost        
    if(wifiSeconds==0){
      Serial.print("mqtt lost!try reconnecting");        
    }
    mqtt->mqttConnect();
    wifiSeconds++;Serial.print(".");    
    if(wifiSeconds>60){
      wifiLost();
    }
  }else if(wifiStatus==9){//ap button push
    setupAp();
  }else if(wifiStatus==10){//ap seaching
    if(wifiSeconds>120){
      wifiStatus=-1;wifiSeconds=0;
      WiFi.mode(WIFI_OFF);
      Serial.println("WiFi abort...see you again");
    }else{
      wifiSeconds++;Serial.print(".");
    }
  }
}
#endif //__ESP32_MQTT_H__
