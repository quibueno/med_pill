
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

#define RESET_BUTTON_PIN 0 // Pino GPIO ao qual o botão está conectado
#define RESET_HOLD_TIME 20000  // Tempo de espera para resetar (20 segundos)

const int stepsPerRevolution = 768;
const int numDeliveryTimes = 3;

Stepper myStepper(stepsPerRevolution, 19, 5, 18, 17);
String medicationDeliveryTimes[numDeliveryTimes];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
DNSServer dns;
Preferences preferences;
WiFiClientSecure client;

unsigned long previousMillis = 0;
const long interval = 1000;  // Interval to check the time (1 second)
int lastDeliveredMinute = -1;

unsigned long buttonPressedTime = 0;  // Marca o momento em que o botão foi pressionado
bool buttonHeld = false;  // Estado para verificar se o botão está sendo mantido pressionado

void setup() {
    Serial.begin(115200);

    // Configura o pino do botão como entrada com resistor pull-up
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    // Chama outras funções de configuração
    setupWiFi();
    setupNTP();
    setupMotor();
    loadDeliveryTimesFromMemory();
    setupWebServer();
}

void loop() {
    // Verifica o estado do botão
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        // Se o botão está pressionado e ainda não foi iniciado o contador
        if (!buttonHeld) {
            buttonHeld = true;
            buttonPressedTime = millis();  // Armazena o momento em que o botão foi pressionado
        }

        // Se o botão está pressionado e o tempo de espera foi atingido
        if (buttonHeld && (millis() - buttonPressedTime >= RESET_HOLD_TIME)) {
            Serial.println("Resetando configurações...");

            // Limpa todas as preferências armazenadas
            preferences.begin("deliveryTimes", false);
            preferences.clear();
            preferences.end();

            // Reseta as configurações de WiFi
            AsyncWiFiManager wifiManager(&server, &dns);
            wifiManager.resetSettings();

            // Reinicia o dispositivo para aplicar as mudanças
            delay(1000);  // Aguarda um pouco para garantir a limpeza dos dados
            ESP.restart();
        }
    } else {
        // Se o botão não está pressionado, reseta o estado
        buttonHeld = false;
    }

    // Continuação do loop principal
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        timeClient.update();

        String currentTime = timeClient.getFormattedTime();
        int currentMinute = timeClient.getMinutes();

        for (int i = 0; i < numDeliveryTimes; i++) {
            if (currentTime == medicationDeliveryTimes[i] && currentMinute != lastDeliveredMinute) {
                deliverMedication();
                lastDeliveredMinute = currentMinute;
            }
        }
    }
}

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    AsyncWiFiManager wifiManager(&server, &dns);
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.autoConnect("Dispenser_AP");
}

void setupNTP() {
    timeClient.begin();
    timeClient.setTimeOffset(-10800);
}

void setupMotor() {
    myStepper.setSpeed(10);
}

void loadDeliveryTimesFromMemory() {
    preferences.begin("deliveryTimes", false);
    for (int i = 0; i < numDeliveryTimes; i++) {
        medicationDeliveryTimes[i] = preferences.getString(String(i), "06:59:00");
    }
    preferences.end();
}

void setupWebServer() {
    server.on("/", HTTP_GET, handleRootRequest);
    server.on("/setTimes", HTTP_POST, handleSetTimesRequest);
    server.begin();
}

void handleRootRequest(AsyncWebServerRequest *request) {
    String botToken = getPreference("botToken", "");
    String chatId = getPreference("chatId", "");

    String html = generateHtmlPage(botToken, chatId);
    request->send(200, "text/html", html);
}

String generateHtmlPage(const String &botToken, const String &chatId) {
    String html = "<html><body><form action='/setTimes' method='POST'>";
    html += "Bot Token: <input type='text' name='botToken' value='" + botToken + "'><br>";
    html += "Chat ID: <input type='text' name='chatId' value='" + chatId + "'><br>";

    for (int i = 0; i < numDeliveryTimes; i++) {
        html += "Horario " + String(i + 1) + ": <input type='text' name='t" + String(i + 1) + "' value='" + medicationDeliveryTimes[i] + "'><br>";
    }

    html += "<input type='submit' value='Salvar'></form></body></html>";
    return html;
}

void handleSetTimesRequest(AsyncWebServerRequest *request) {
    if (validRequestParameters(request)) {
        for (int i = 0; i < numDeliveryTimes; i++) {
            medicationDeliveryTimes[i] = request->getParam("t" + String(i + 1), true)->value();
            setPreference(String(i), medicationDeliveryTimes[i]);
        }
        setPreference("botToken", request->getParam("botToken", true)->value());
        request->send(200, "text/plain", "Horários atualizados com sucesso");
    } else {
        request->send(400, "text/plain", "Parâmetros inválidos");
    }
}

bool validRequestParameters(AsyncWebServerRequest *request) {
    for (int i = 1; i <= numDeliveryTimes; i++) {
        if (!request->hasParam("t" + String(i), true)) {
            return false;
        }
    }
    return request->hasParam("botToken", true);
}

void deliverMedication() {
    myStepper.step(stepsPerRevolution);
    delay(2000);  // Considere reduzir ou eliminar este delay.
    myStepper.step(-stepsPerRevolution);
    sendTelegramMessage("Medicamento entregue");
}

void sendTelegramMessage(const String &message) {
    String botToken = getPreference("botToken", "");
    String chatId = getPreference("chatId", "");

    if (botToken.isEmpty() || chatId.isEmpty()) {
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

String getPreference(const String &key, const String &defaultValue) {
    preferences.begin("deliveryTimes", false);
    String value = preferences.getString(key, defaultValue);
    preferences.end();
    return value;
}

void setPreference(const String &key, const String &value) {
    preferences.begin("deliveryTimes", false);
    preferences.putString(key, value);
    preferences.end();
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
