
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

#include <WiFi.h>
#include <esp_task_wdt.h>
#include <Stepper.h>
#include <SPI.h>
#include <NTPClient.h>//Biblioteca do NTP.
#include <WiFiUdp.h>//Biblioteca do UDP.
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>


TaskHandle_t Task1;
TaskHandle_t Task2;

const int stepsPerRevolution = 768;  // Numero de passos no motor para 21 posições

const char* ssid = "<nome>"; // Nome SSID - WiFi
const char* pass = "<Password>"; // Nome Password - WiFi

//watchdog
#define WDT_TIMEOUT 1200000
void(* resetFunc) (void) = 0;

// token Telegram

#define BOTtoken "<Hash Telegram>" // Hash Bot Telegram
const char* CHAT_ID_01 = "<ID usuário 1>"; // ID usuário 01
const char* CHAT_ID_02 = "<ID usuário 2>"; // ID usuário 01
const char* CHAT_ID_03 = "<ID usuário 3>"; // ID usuário 01
const char* server_02 = "api.telegram.org"; // Endereço API Telegram


// token IFTTT

#define webhookskey "<Hash IFTT>"            //Webhook IFTTT
#define eventname1 "<Evento>"               // Evento IFTTT 
const char* trigger1 = "/trigger/" eventname1 "/with/key/" webhookskey;

// IFTT
char const* event = trigger1;
const char* server = "maker.ifttt.com"; // Endereço API IFTTT
const int port = 80;
const int httpsPort = 443;
const int API_TIMEOUT = 15000;
bool result = true;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
int Bot_mtbs = 1000;
long Bot_lasttime;

String hora;  // String para informar horario - Debug

WiFiUDP ntpUDP;
WiFiClient net_pill;

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);


// ULN2003 Motor Driver Pins
#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17
// REED e BUZZER Pins

#define BUZZ 25
#define REED 26

// booting
int i = 0;
int x = 0;
int last = millis();

//variavel de tempo
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// initialize the stepper library
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

// Conexão com rede Wi-FI
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  IPAddress localIP = WiFi.localIP();
}


void setup() {
  Serial.begin(115200);
  delay(50);
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  timeClient.begin();
  timeClient.setTimeOffset(-10800);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  myStepper.setSpeed(10);
  pinMode (REED, INPUT);
  pinMode (BUZZ, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
   //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
    delay(500); 
}

// Core 0 no ESP32 com finalidade de execução do NTP e WiFi Reconnect.

void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
 //   hora = timeClient.getFormattedTime();
    delay(1000);
    timeClient.update();  // Update ntp a cada 60 segundos
     if (millis() - last >=2000 && x < 5)
 {
  last = millis();
  i++;
  if (x == 5){
    Serial.printf("ok 3s");
  }
 }
 unsigned long currentMillis = millis();  // Verifica conexão WiFi e Reconecta cada haja perda de conexão
 //  if WiFi is down, try; // reconnecting every CHECK_WIFI_TIME seconds
   if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    // lgbq
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
     WiFi.disconnect();
     WiFi.reconnect();
     previousMillis = currentMillis;
  }
  } 
}

// Core 1 no ESP32 com finalidade de acesso ao step motor e alertas
void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    inicial();
  
   if (timeClient.getFormattedTime() == "06:59:00")//Se a hora atual for igual à que definimos, girar roda
   {
      music();
      gaveta();
      telegram();
      delay(600000);
      pote();
   }
   
   if (timeClient.getFormattedTime() == "11:59:00")//Se a hora atual for igual à que definimos, girar roda
   {
      music();
      gaveta();
      telegram();
      delay(600000);
      pote();
   }
   if (timeClient.getFormattedTime() == "18:59:00")//Se a hora atual for igual à que definimos, girar roda
   {
      music(); // Executa alerta via Alexa
      gaveta(); // Entrega medicamento e toca alarme atá a retirada.
      telegram(); // Envia comunicado ao BOT Telegram informando que o medicamento foi retirado.
      delay(600000); // Aguarda 15 min
      pote(); // Caso o pote de medicamento não seja colocado no equipamento gera um alarme 
   }
   
   digitalWrite(LED_BUILTIN, HIGH);
   delay(500);
   digitalWrite(LED_BUILTIN, LOW);
   delay(500);
 }
}

void loop() {
  }

void gaveta() {
  myStepper.step(stepsPerRevolution); // Aciona o step motor para 768 passos 
  if (digitalRead(REED)== HIGH); {  // Alarme para retirar medicamento
     do
     {
       digitalWrite(BUZZ, HIGH);
       delay(250);
       digitalWrite(BUZZ, LOW);
       delay(100);
       digitalWrite(BUZZ, HIGH);
       delay(250);
       digitalWrite(BUZZ, LOW);
       delay(100);
       digitalWrite(BUZZ, HIGH);
       delay(500);
       digitalWrite(BUZZ, LOW);
       delay(100);
     } while (digitalRead(REED) == LOW);
   }
}

void pote () {   // Alarme para colocar o pote do medicamento no local
  if (digitalRead(REED)== LOW); {
     do
     {
       digitalWrite(BUZZ, HIGH);
       delay(100);
       digitalWrite(BUZZ, LOW);
       delay(100);
       digitalWrite(BUZZ, HIGH);
       delay(100);
       digitalWrite(BUZZ, LOW);
       delay(100);
       digitalWrite(BUZZ, HIGH);
       delay(100);
       digitalWrite(BUZZ, LOW);
       delay(100);
       led();
     } while (digitalRead(REED) == HIGH);
   }
}

void telegram() {
  Serial.print("Connecting to ");
  Serial.print(server_02);

  WiFiClient client;

  for (int tries = 0; tries < 5; tries++) {        //Faça 5 tentativas de acesso ao site antes de enviar o comunicado
    client.setTimeout(API_TIMEOUT);
    if(client.connect(server_02, port)) break;   // Sair do loop caso não haja resposta
    Serial.print(".");                           
    delay(1000);
  }

  Serial.println();
  if(!client.connected()) {
    Serial.println("Failed to connect to server");
    client.stop();
    return;                                      
  }
  Serial.print("Request event: ");               //Executar o comando.
  Serial.println(event);
  bot.sendMessage(CHAT_ID_01, "Pegou remedio", "");  // Comando para enviar mensagem via telegram
  delay(1000);
  bot.sendMessage(CHAT_ID_02, "Pegou remedio", "");
  delay(1000);
  bot.sendMessage(CHAT_ID_03, "Pegou remedio", "");
  int timeout = 50;                              //Aguardar 5 segundos para fechar conexão
  while(!client.available() && (timeout-- > 0)){
    delay(100);
  }

  if(!client.available()) {
     Serial.println("No response to GET");
     client.stop();
     return;
  }
  while(client.available()){
    Serial.write(client.read());
  }
  Serial.println("\nclosing connection");
  delay(1000);
  client.stop();
}


void music(){
  Serial.print("Connecting to ");
  Serial.print(server);

  WiFiClientSecure client;

  for (int tries = 0; tries < 5; tries++) {        /Faça 5 tentativas de acesso ao site antes de enviar o comunicado
    client.setTimeout(API_TIMEOUT);
    client.setInsecure();
    if(client.connect(server, httpsPort)) break;   // Sair do loop caso não haja resposta
    Serial.print(".");                            
    delay(2000);
  }

  Serial.println();
  if(!client.connected()) {
    Serial.println("Failed to connect to server");
    client.stop();
    return;                                      
  }
  Serial.print("Request event: ");               // Comando para enviar mensagem via IFTTT
  Serial.println(event);
  client.print(String("GET ") + event +
                  " HTTP/1.1\r\n" +
                  "Host: " + server + "\r\n" +
                  "Connection: close\r\n\r\n");

  int timeout = 50;                              
  while(!client.available() && (timeout-- > 0)){
    delay(100);
  }

  if(!client.available()) {
     Serial.println("No response to GET");
     client.stop();
     return;
  }
  while(client.available()){
    Serial.write(client.read());
  }
  Serial.println("\nclosing connection");
  delay(1000);
  client.stop();
}


void led() {
digitalWrite(LED_BUILTIN, HIGH);
delay(50);
digitalWrite(LED_BUILTIN, LOW);
delay(50);
}

void inicial() { // Gera alerta sonoro quando houver conexão a rede WiFi.
    if (millis() - last >= 2000 && i < 2) {
      Serial.println("inicial booting...");
      digitalWrite(BUZZ, HIGH);
       delay(500);
       digitalWrite(BUZZ, LOW);
       delay(100);
      last = millis();
      i++;
      if (i == 2) {
        Serial.println("in 3s");
      }
  }
}
