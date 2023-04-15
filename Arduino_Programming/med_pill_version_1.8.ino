
/*
MED_PILL -- Medicamental Dispenser
Released under the GPLv3 license. 2015, 
Copyright (c) 2022 by Luiz Gustavo Bueno Quirino
https://github.com/quibueno/med_pill

This file is part of the Med_Pill Library

This Library is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the Arduino Med_Pill Library.  If not, see
<http://www.gnu.org/licenses/>.
*/
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Stepper.h>
#include <Preferences.h>
#include <UniversalTelegramBot.h>

const int stepsPerRevolution = 768;
Stepper myStepper(stepsPerRevolution, 19, 5, 18, 17);

String medicationDeliveryTimes[3];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

AsyncWebServer server(80);
DNSServer dns;

int lastDeliveredMinute = -1;

Preferences preferences;

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);

  // Configuração do WiFi
  WiFi.mode(WIFI_STA);
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect("Dispenser_AP");

  // Configuração do NTP
  timeClient.begin();
  timeClient.setTimeOffset(-10800);

  // Configuração do motor
  myStepper.setSpeed(10);

  // Carregar horários da memória não volátil
  preferences.begin("deliveryTimes", false);
  for (int i = 0; i < 3; i++) {
    medicationDeliveryTimes[i] = preferences.getString(String(i).c_str(), "06:59:00");
  }
  preferences.end();

  // Configuração do servidor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin("deliveryTimes", false);
    String botToken = preferences.getString("botToken", "");
    String chatId = preferences.getString("chatId", "");
    preferences.end();

    String html = "<html><body><form action='/setTimes' method='POST'>";
    html += "Bot Token: <input type='text' name='botToken' value='" + botToken + "'><br>";
    html += "Chat ID: <input type='text' name='chatId' value='" + chatId + "'><br>";
    html += "Horario 1: <input type='text' name='t1' value='" + medicationDeliveryTimes[0] + "'><br>";
    html += "Horario 2: <input type='text' name='t2' value='" + medicationDeliveryTimes[1] + "'><br>";
    html += "Horario 3: <input type='text' name='t3' value='" + medicationDeliveryTimes[2] + "'><br>";
    html += "<input type='submit' value='Salvar'></form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/setTimes", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("t1", true) && request->hasParam("t2", true) && request->hasParam("t3", true) &&
        request->hasParam("botToken", true) && request->hasParam("chatId", true)) {
      medicationDeliveryTimes[0] = request->getParam("t1", true)->value();
      medicationDeliveryTimes[1] = request->getParam("t2", true)->value();
      medicationDeliveryTimes[2] = request->getParam("t3", true)->value();

            // Salvar horários, BOT_TOKEN e CHAT_ID na memória não volátil
      preferences.begin("deliveryTimes", false);
      for (int i = 0; i < 3; i++) {
        preferences.putString(String(i).c_str(), medicationDeliveryTimes[i]);
      }
      preferences.putString("botToken", request->getParam("botToken", true)->value());
      preferences.putString("chatId", request->getParam("chatId", true)->value());
      preferences.end();

      request->send(200, "text/plain", "Horários atualizados");
    } else {
      request->send(400, "text/plain", "Parâmetros inválidos");
    }
  });

  server.begin();
}

void loop() {
  timeClient.update();
  String currentTime = timeClient.getFormattedTime();
  int currentMinute = timeClient.getMinutes();

  for (int i = 0; i < 3; i++) {
    if (currentTime == medicationDeliveryTimes[i] && currentMinute != lastDeliveredMinute) {
      deliverMedication();
      lastDeliveredMinute = currentMinute;
    }
  }

  delay(1000);
}

void deliverMedication() {
  myStepper.step(stepsPerRevolution);
  delay(2000);
  myStepper.step(-stepsPerRevolution);
  sendTelegramMessage("Medicamento entregue");
}

void sendTelegramMessage(const String &message) {
  preferences.begin("deliveryTimes", false);
  String botToken = preferences.getString("botToken", "");
  String chatId = preferences.getString("chatId", "");
  preferences.end();

  if (botToken == "" || chatId == "") {
    Serial.println("Bot Token ou Chat ID não configurados");
    return;
  }

  UniversalTelegramBot bot(botToken, client);

  if (bot.sendSimpleMessage(chatId, message, "Markdown")) {
    Serial.println("Mensagem enviada com sucesso");
  } else {
    Serial.println("Falha ao enviar a mensagem");
  }
}

void configModeCallback(AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}



/* Neste código, um portal cativo é criado quando o ESP32 é iniciado, permitindo que o usuário se conecte à rede "Dispenser_AP" e acesse o servidor web no ESP32. A página inicial do servidor web exibe um formulário para inserir os horários de entrega de medicamentos.

Os horários são armazenados na memória não volátil do ESP32, usando a biblioteca Preferences, para que não se percam caso o ESP32 seja desligado. Além disso, o servidor web também inclui um ponto de extremidade `/setTimes` para salvar os horários atualizados.

 Para configurar o horário e a rede Wi-Fi:

 1. Ligue o ESP32.
 2. Conecte-se à rede Wi-Fi "Dispenser_AP" com um dispositivo.
 3. Abra um navegador e acesse o endereço IP `192.168.4.1`.
 4. Insira os horários de entrega de medicamentos nos campos de texto e clique em "Salvar".
 5. Desconecte-se da rede "Dispenser_AP" e conecte-se à sua rede Wi-Fi padrão. O ESP32 deve se conectar automaticamente a essa rede.

 Lembre-se de instalar as bibliotecas necessárias (ESPAsyncWebServer, ESPAsyncWiFiManager e ArduinoJson) no Arduino IDE antes de fazer o upload do código.
*/
