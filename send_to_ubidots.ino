/****************************************
 * BIBLIOTECAS
 ****************************************/
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>  //responsável pela comunicação i2c
#include <PubSubClient.h>
#include <LoRa.h>
#include "SSD1306Wire.h"

/****************************************
 * CONSTANTES E VARIAVEIS
 ****************************************/
#define WIFISSID "NOME DA REDE" // Nome da rede
#define PASSWORD "PASSWORD" // sennha da rede
#define TOKEN "A1E-NhiD6FRXsKjgXv5WfmKL9KF4jMCWUa" // TOKEN do ubidots
#define MQTT_CLIENT_NAME "energylab" // NOME do cliente MQQT
#define VARIABLE_LABEL_1 "pH" // label no ubidots da variavel 1
#define VARIABLE_LABEL_2 "Temperatura_agua" // label no ubidots da variavel 2
#define VARIABLE_LABEL_3 "Temperatura_ar" // label no ubidots da variavel 3
#define VARIABLE_LABEL_4 "Umidade_ar" // label no ubidots da variavel 4
#define VARIABLE_LABEL_5 "Oxigenio_Disssolvido" // label no ubidots da variavel 5
#define DEVICE_LABEL "esp32" // label do DEVICE no ubidots onde estão as variaveis
#define SS      18
#define RST     14
#define DI0     26

char mqttBroker[]  = "things.ubidots.com";//define o broker do ubidots
char payload[700];
char topic[150]; //define o tamanho do tópico

//variaveis a serem enviadas para o ubidots no formato string
char str_temp_ar[10];
char str_temp_agua[10];
char str_umi_ar[10];
char str_pH[10];
char str_oxig[10];

//Variaveis sensores
float Ftemp_ar; 
float Fumi_ar;
float Ftemp_agua;
float FpH;
float Foxig;

SSD1306Wire display(0x3c, 4, 15); //define o endereço do display e pinagem **4 e 15 pinos do display**

WiFiClient ubidots;
PubSubClient client(ubidots);

void callback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);
  Serial.write(payload, length);
  Serial.println(topic);
}

void reconnect() {
  while (!client.connected()) {  //loop para conectar mqqt
    Serial.println("Attempting MQTT connection...");
    
    if (client.connect(MQTT_CLIENT_NAME, TOKEN, "")) { //tentativa de conexão
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}


void setup() {

  //seta os estados dos pinos relacionados ao OLED
  pinMode(16,OUTPUT); //RST do oled
  pinMode(25,OUTPUT);
  digitalWrite(16, LOW); // reseta o OLED
  delay(50); 
  digitalWrite(16, HIGH); // enquanto o OLED estiver ligado GPIO16 deve estar HIGH
  
  //configuração display
  display.init(); //inicializa o display
  display.flipScreenVertically(); 
  display.setFont(ArialMT_Plain_10);
  
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  
  //inicia conexão lora com a frequencia de trabalho outras frequencias 433E6, 868E6, 915E6
  //Verifica se está conectando 
  if (!LoRa.begin(868E6)) { 
    Serial.println("Inicialização da LORA falhou!");
    display.drawString(0, 0, "Inicial  LORA falhou!");
    while (1);
  } 
  display.clear();
  display.drawString(0, 0, "Lora iniciou com sucesso!");
  display.display(); //mostra o conteúdo na tela
  delay(1000);
  //else {
  //display.drawString(0, 0, "LoRa Initial success!");}  
  

  Serial.begin(115200); //inicia serial***
  Serial.println(WIFISSID);
  Serial.println(PASSWORD);
  WiFi.begin(WIFISSID, PASSWORD); //inicia o wifi
  Serial.println();
  Serial.print("Wait for WiFi..."); 
  while (WiFi.status() != WL_CONNECTED) { //loop de espera de conexão do wifi
  Serial.print(".");
  delay(500);
  }
  
  Serial.println("");
  Serial.println("WiFi Conectado"); 
  Serial.println("Endereço de IP: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqttBroker, 1883); //processamento mqqt..porta do ubidots 1883
  client.setCallback(callback);  
}

void loop() {
  if (!client.connected()) {//se mqqt não estiver conectado ele reconecta ao servidor
    reconnect();
  }

  String pH = ""; //cria e limpa o pacote de pH
  String temp_agua = ""; //cria e limpa o pacote de temperatura da agua
  String temp_ar = ""; //cria e limpa o pacote de temperatura do ar
  String umi_ar = ""; //cria e limpa o pacote de umidade do ar
  String oxig = ""; // cria e limpa o pacote de oxigenio
  
  int packetSize = LoRa.parsePacket();
  //analisa se existe algo no pacote
  if (packetSize) {
   Serial.println("Pacote recebido ");
    // lê o pacote
   String pacote = ""; // limpa o pacote
   while (LoRa.available()) {
    pacote+= (char)LoRa.read();
   }
   int j=0;
   int i=0;
   while(pacote[i]!='x' && i <pacote.length()) {
     if(pacote[i]!='q'){
       if (j==0){
         pH+=pacote[i];  
       } 
       if (j==1){
         temp_agua+=pacote[i];  
       } 
       if (j==2){
         temp_ar+=pacote[i];  
       }  
       if (j==3){
        umi_ar+=pacote[i];  
       }  
       if (j==4){
         oxig+=pacote[i];  
       }  
     }
    else{
      j++;
    } 
    i++;
  }   
  
   Serial.print("pH: "); 
   Serial.println(pH); 
   Serial.print("Temp agua: "); 
   Serial.println(temp_agua); 
   Serial.print("temp ar: "); 
   Serial.println(temp_ar); 
   Serial.print("umi ar: "); 
   Serial.println(umi_ar); 
   Serial.print("oxi: "); 
   Serial.println(oxig); 

   
   
   FpH = pH.toFloat();
   Ftemp_agua = temp_agua.toFloat();
   Ftemp_ar = temp_ar.toFloat();
   Fumi_ar = umi_ar.toFloat();
   Foxig = oxig.toFloat();
   delay(1);

   //-----------FAKE DATA ----------
   FpH = 1;
   Ftemp_agua = 2;
   Ftemp_ar = 3;
   Fumi_ar = 4;
   Foxig = 5;
   //-----------END FAKE DATA ----------
  }
  
  
  dtostrf(FpH, 4, 2, str_pH); //transforma variavel em string - 4 é a largura e 2 a precisão
  dtostrf(Ftemp_agua, 4, 2, str_temp_agua);
  dtostrf(Ftemp_ar, 4, 2, str_temp_ar);
  dtostrf(Fumi_ar, 4, 2, str_umi_ar);
  dtostrf(Foxig, 4, 2, str_oxig);


  display.clear();
  display.drawString(0, 0, "SENSORES DO LAGO");
  display.drawString(0, 10, "pH: ");
  display.drawString(20, 10,String(FpH));
  display.drawString(0, 20, "Temp. água: ");
  display.drawString(64, 20, String(Ftemp_agua));
  display.drawString(94, 20, "ºC");
  display.drawString(0, 30, "Temp. ar:");
  display.drawString(49, 30, String(Ftemp_ar));
  display.drawString(80, 30, "ºC");
  display.drawString(0, 40, "Umidade ar:");
  display.drawString(60, 40, String(Fumi_ar));
   display.drawString(90, 40, "%");
  display.drawString(0, 50, "OD:");
  display.drawString(21, 50, String(Foxig));
  display.drawString(50, 50, "g/L");
  display.display(); //mostra o conteúdo na tela 
  
  sprintf(topic, "%s", ""); // limpa o conteudo do tópico
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);//determina o endpoint das variaveis no tópico

  sprintf(payload, "%s", ""); // limpa o payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_1); // adiciona a variavel ao payload  
  sprintf(payload, "%s {\"value\": %s", payload, str_pH); // adiciona um valor a variavel
  sprintf(payload, "%s } }", payload); // fecha os suporte dos dicionários -- conteudo do json para ser enviado
  client.publish(topic, payload);
  
  sprintf(topic, "%s", ""); // limpa o conteudo do tópico
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);//determina o endpoint das variaveis no tópico

  sprintf(payload, "%s", ""); // limpa o payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_2); // adiciona a variavel ao payload  
  sprintf(payload, "%s {\"value\": %s", payload, str_temp_agua); // adiciona um valor a variavel
  sprintf(payload, "%s } }", payload); // fecha os suporte dos dicionários -- conteudo do json para ser enviado
  client.publish(topic, payload);

  sprintf(topic, "%s", ""); 
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);

  sprintf(payload, "%s", ""); 
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_3);  
  sprintf(payload, "%s {\"value\": %s", payload, str_temp_ar); 
  sprintf(payload, "%s } }", payload);
  client.publish(topic, payload);

  sprintf(topic, "%s", ""); 
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);

  sprintf(payload, "%s", ""); 
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_4);  
  sprintf(payload, "%s {\"value\": %s", payload, str_umi_ar); 
  sprintf(payload, "%s } }", payload);
  client.publish(topic, payload);

  sprintf(topic, "%s", ""); 
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);

  sprintf(payload, "%s", ""); 
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_5);  
  sprintf(payload, "%s {\"value\": %s", payload, str_oxig); 
  sprintf(payload, "%s } }", payload);
  client.publish(topic, payload);

  client.loop();
  Serial.println("Enviado para o ubidots...");
  delay(1);
}
