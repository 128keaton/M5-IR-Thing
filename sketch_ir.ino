#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
#include <ESP32MQTTClient.h>
#include <M5Unified.h>

#define SEND_PWM_BY_TIMER
#define IR_TX_PIN 44

#include "M5Cardputer.h"
#include <IRremote.hpp>


bool showingIntro = true;
bool mqttActive = false;
auto loopCounter = 0;

char *server = "mqtt://10.0.1.212:1883";
char *subscribeTopic = "ir/receiver/#";
ESP32MQTTClient mqttClient;


void drawIntro() {
  showingIntro = true;
  M5Cardputer.Display.clear();
  M5Cardputer.Display.drawString("Press a Key", M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextColor(GREEN);
  M5Cardputer.Display.setTextDatum(middle_center);
  M5Cardputer.Display.setTextSize(1);

  IrSender.begin(DISABLE_LED_FEEDBACK);  // Start with IR_SEND_PIN as send pin
  IrSender.setSendPin(IR_TX_PIN);

  M5Cardputer.Display.clear();
  M5Cardputer.Display.drawString("Connecting to WiFi", M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);
  Serial.println("Connecting to WiFi");

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.begin("uuuuhhhhhhhh", "yuhfiddy");


  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    M5Cardputer.Display.drawString("Connecting to WiFi.", M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);
    Serial.println("Connecting to WiFi.");
    delay(1000);
  }

  Serial.println("Connected to WiFi");

  mqttClient.enableDebuggingMessages();

  mqttClient.setURI(server, "tibbo", "tibbo");
  mqttClient.enableLastWillMessage("ir/receiver/state/offline", "true");
  mqttClient.loopStart();
  drawIntro();
}


void drawSendSIRC5Command(uint8_t command, String display, uint16_t repeats = 3) {
  M5Cardputer.Display.clear();
  M5Cardputer.Display.drawString(display, M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);

  uint16_t address = 0x30 & 0xFF;

  showingIntro = false;
  Serial.println();
  Serial.print(F("Sending SIRC5: address=0x"));
  Serial.print(address, HEX);
  Serial.print(F(", command=0x"));
  Serial.print(command, HEX);
  Serial.println();

  IrSender.sendSony(address, command, repeats, SIRCS_15_PROTOCOL);
  delay(45);
}

void drawSendSIRC20Command(uint8_t sirc20command, uint16_t sirc20Address, String display) {
  M5Cardputer.Display.clear();
  M5Cardputer.Display.drawString(display, M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);

  uint16_t address = sirc20Address & 0x0FFF;
  uint16_t command = sirc20command & 0x7F;

  showingIntro = false;
  Serial.println();
  Serial.print(F("Sending SIRC20: address=0x"));
  Serial.print(address, HEX);
  Serial.print(F(", command=0x"));
  Serial.print(command, HEX);
  Serial.println();

  IrSender.sendSony(address, command, 3, SIRCS_20_PROTOCOL);
  delay(50);
}


void powerOff() {
  uint8_t command = 47;
  drawSendSIRC5Command(command, "Power off (discrete)");
  mqttClient.publish("ir/receiver/state/on", "false", 0, false);
}

void powerOn() {
  uint8_t command = 46;
  drawSendSIRC5Command(command, "Power on", 4);
  mqttClient.publish("ir/receiver/state/on", "true", 0, false);
}

void volUp() {
  uint8_t command = 18;
  drawSendSIRC5Command(command, "Vol Up");
  mqttClient.publish("ir/receiver/state/mute", "false", 0, false);
}

void volDown() {
  uint8_t command = 19;
  drawSendSIRC5Command(command, "Vol Down");
  mqttClient.publish("ir/receiver/state/mute", "false", 0, false);
}


void mute() {
  uint8_t command = 20;
  drawSendSIRC5Command(command, "Mute");
  mqttClient.publish("ir/receiver/state/mute", "true", 0, false);
}

void sourceMediabox() {
  uint8_t command = 64;
  uint16_t sirc20Address = 0x0D10;
  drawSendSIRC20Command(command, sirc20Address, "Media Box");
  mqttClient.publish("ir/receiver/state/source", "mediaBox", 0, false);
}

void sourcePC() {
  uint8_t command = 22;
  uint16_t sirc20Address = 0x0510;
  drawSendSIRC20Command(command, sirc20Address, "PC");
  mqttClient.publish("ir/receiver/state/source", "PC", 0, false);
}

void sourceCD() {
  uint8_t command = 37;
  drawSendSIRC5Command(command, "CD");
  mqttClient.publish("ir/receiver/state/source", "CD", 0, false);
}

void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client) {
  if (mqttClient.isMyTurn(client)) {
    mqttActive = true;
    mqttClient.publish("ir/receiver/state/offline", "false", 0, false);
    mqttClient.subscribe(subscribeTopic, [](const String &topic, const String &payload) {
      Serial.println(topic);
      Serial.println(payload);

      if (topic == "ir/receiver/source") {
        if (payload == "PC")
          sourcePC();
        else if (payload == "mediaBox")
          sourceMediabox();
        else if (payload == "CD")
          sourceCD();
      } else if (topic == "ir/receiver/on") {
        if (payload == "true")
          powerOn();
        else if (payload == "false")
          powerOff();
      } else if (topic == "ir/receiver/volUp")
        volUp();
      else if (topic == "ir/receiver/volDown")
        volDown();
      else if (topic == "ir/receiver/mute")
        mute();
    });
  }
}


void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isKeyPressed('p')) {
    powerOff();
  } else if (M5Cardputer.Keyboard.isKeyPressed('b')) {
    powerOn();
  } else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    volUp();
  } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    volDown();
  } else if (M5Cardputer.Keyboard.isKeyPressed('m')) {
    mute();
  } else if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    sourceMediabox();
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    sourcePC();
  } else if (M5Cardputer.Keyboard.isKeyPressed('3')) {
    sourceCD();
  } else if (!M5Cardputer.Keyboard.isPressed() && !showingIntro) {
    drawIntro();
  }

  if (loopCounter >= 30000) {
    loopCounter = 0;
    mqttClient.publish("ir/receiver/state/offline", "false", 0, false);
  } else
    loopCounter++;

  delay(1);
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
  return ESP_OK;
}
