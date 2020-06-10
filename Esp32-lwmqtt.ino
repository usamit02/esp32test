

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
#include "esp32-mqtt.h"
#include <Arduino_JSON.h>
void setup()
{
  Serial.begin(115200);

  pinMode(14, OUTPUT);
  pinMode(25, OUTPUT);
  pinMode(27, OUTPUT);
  ledcSetup(0, 12800, 8);
  ledcAttachPin(12, 0);
  digitalWrite(27, 1);
  delay(500);
  digitalWrite(27, 0);
  setupCloudIoT();
  delay(10); // <- fixes some issues with WiFi stability
  if (!mqttClient->connected())
  {
    connect();
  }
}

unsigned long lastMillis = 0;
unsigned int led27 = 0;
void loop()
{
  mqtt->loop();
  delay(10); // <- fixes some issues with WiFi stability

  if (!mqttClient->connected())
  {
    connect();
  }
  if (millis() - lastMillis > 1000)
  {
    lastMillis = millis();
    thermoRead();
    led27 = led27 ? 0 : 1;
    digitalWrite(27, led27);
  }
}
void logging(String msg)
{

  publishTelemetry(msg); // publish処理(送信処理)
  Serial.println(msg);
}
void messageReceived(String &topic, String &payload)
{
  JSONVar com = JSON.parse(payload);
  if (com.hasOwnProperty("led_green"))
  {
    digitalWrite(14, (int)com["led_green"]);
  }
  if (com.hasOwnProperty("led_red"))
  {
    unsigned int brightness = (int)com["led_red"];
    brightness = brightness > 255 ? 255 : brightness;
    ledcWrite(0, brightness);
  }
}
void thermoRead()
{
  float R0 = 10000.0;
  float R1 = 10000.0;
  int adc;
  int adcsum = 0;
  int adcnum = 0;
  digitalWrite(25, 1);
  delay(10);
  for (int i = 0; i < 3; i++)
  {
    adc = analogRead(A0);
    if (adc > 500 && adc < 3500)
    {
      adcsum += adc;
      adcnum++;
    }
  }
  if (adcnum)
  {
    adc = adcsum / adcnum;
    float V = (float)adc * 3.3 / (4096 * 1.0); //0.9=補正係数
    digitalWrite(25, 0);
    float RT = (3.3 / V - 1) * R1;                      //(V / (3.3 - V)) * R1;                    //サーミスタ抵抗値
    float C = 1 / (log(RT / R0) / 3950 + (1 / 298.15)); //B定数3950
    float TC = C - 273.15;
    Serial.println("adc:" + String(adc) + "  ," + String(TC) + "C");
  }
  else
  {
    Serial.println("サーミスタ異常");
  }
}