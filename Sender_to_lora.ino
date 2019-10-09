/**********************************
  BIBLIOTECAS
***********************************/
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  //responsável pela comunicação i2c
//#include "SSD1306Wire.h"
#include <OneWire.h> //
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
//bibliotecas do sensor de oxigenio
#if (defined(__AVR__))
#include <avr\pgmspace.h>
#else
#include <pgmspace.h>
#endif
#include <EEPROM.h>
/**********************************
  CONSTANTES E VARIAVEIS
***********************************/
#define DHTTYPE DHT11 //sensor temp e hum
#define SS      18
#define RST     14
#define DI0     26
#define ONE_WIRE_BUS 22 //sensor temp agua
#define measure 38 // ph
#define pinBat 13 //pino leitura tensão bateria

//************************* OXIGÊNIO *******************////////////////
#define DoSensorPin 39    //dissolved oxygen sensor analog output pin to arduino mainboard
#define VREF 3300    //for arduino uno, the ADC reference is the AVCC, that is 5000mV(TYP)
float doValue;      //current dissolved oxygen value, unit; mg/L

#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) EEPROM.write(address+i, pp[i]);}
#define EEPROM_read(address, p)  {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) pp[i]=EEPROM.read(address+i);}

#define ReceivedBufferLength 20
char receivedBuffer[ReceivedBufferLength + 1];  // store the serial command
byte receivedBufferIndex = 0;

#define SCOUNT  30           // sum of sample point
int analogBuffer[SCOUNT];    //store the analog value in the array, readed from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;

#define SaturationDoVoltageAddress 12          //the address of the Saturation Oxygen voltage stored in the EEPROM
#define SaturationDoTemperatureAddress 16      //the address of the Saturation Oxygen temperature stored in the EEPROM
float SaturationDoVoltage, SaturationDoTemperature;
float averageVoltage;

const float SaturationValueTab[41] PROGMEM = {      //saturation dissolved oxygen concentrations at various temperatures
  14.46, 14.22, 13.82, 13.44, 13.09,
  12.74, 12.42, 12.11, 11.81, 11.53,
  11.26, 11.01, 10.77, 10.53, 10.30,
  10.08, 9.86,  9.66,  9.46,  9.27,
  9.08,  8.90,  8.73,  8.57,  8.41,
  8.25,  8.11,  7.96,  7.82,  7.69,
  7.56,  7.43,  7.30,  7.18,  7.07,
  6.95,  6.84,  6.73,  6.63,  6.53,
  6.41,
}; //************************* OXIGÊNIO *******************////////////////

const int DHTPin = 21;
float pH = 0;
//float measure = 0;
float h;
float t;
float temp_agua = 0;
float oxig = 0;
float tensaoBat = 0;
int porBat = 0; 

//configura componentes de algumas bibliotecas
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire); //config sensor da agua
DeviceAddress sensor1; //endereço sensor da agua  // rever sensor
DHT dht(DHTPin, DHTTYPE);
//SSD1306Wire display(0x3c, 4, 15); //define o endereço do display e pinagem **4 e 15 pinos do display**

//WATCHDOG AU AU
hw_timer_t *timer = NULL; //faz o controle do temporizador (interrupção por tempo)

void IRAM_ATTR resetModule() {
  ets_printf("(watchdog) reiniciar\n"); //imprime no log
  esp_restart_noos(); //reinicia o chip
}

void setup() {
  //timer
  timer = timerBegin(0, 80, true); //timerID 0, div 80
  timerAttachInterrupt(timer, &resetModule, true); //timer, callback, interrupção de borda
  timerAlarmWrite(timer, 10000000, true);    //timer, tempo (us), repetição
  timerAlarmEnable(timer); //habilita a interrupção
  
  //Configura estado dos pinos
  pinMode(16, OUTPUT); //RST do oled
  pinMode(25, OUTPUT);
  digitalWrite(16, LOW); // reseta o OLED
  delay(1000);
  digitalWrite(16, HIGH); // enquanto o OLED estiver ligado GPIO16 deve estar HIGH

//  display.init(); //inicializa o display
//  display.flipScreenVertically();
//  display.setFont(ArialMT_Plain_10);


  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  Serial.begin(115200);
  dht.begin(); // inicia a biblioteca do dht11
  sensors.begin(); //inicia função sensor do ds18b20

  pinMode(DoSensorPin, INPUT);
  readDoCharacteristicValues(); //Le os valores salvos de oxigenio na EEPROM


  while (!Serial);
  Serial.println("SENSORES LAGO");

  //Verifica se a lora iniciou corretamente..Frequencias possiveis: 433E6, 868E6, 915E6
//  display.clear();
  if (!LoRa.begin(868E6)) {
    Serial.println("Incialização da LoRa falhou");
//    display.drawString(0, 0, "Inicial LoRa falhou!");
    while (1);
  }
  else {
//    display.drawString(0, 0, "LoRa iniciou com sucesso!!");
      Serial.println("LoRa iniciou com sucesso!!");
  }
//  display.display(); //mostra o conteúdo na tela

}

void loop() {
  
  timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog)
  
//  display.clear();
//  display.drawString(0, 0, " -- SENSORES DO LAGO --");
//  display.drawString(0, 10, "   -- UNIVERSIDADE --");
//  display.drawString(0, 20, "      -- POSITIVO --");
//  display.display(); //mostra o conteúdo na tela
  
  //****** SENSOR DE TEMPERATURA DA AGUA DS18B20 **************//
  sensors.requestTemperatures(); // Send the command to get temperature readings
  temp_agua = sensors.getTempCByIndex(0);

  //****** SENSOR DE TEMPERATURA E UMIDADE DHT11  **************//
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) { //VERIFICA E INFORMA SE ESTA FUNCIONANDO
    Serial.println("Falha ao ler  o sensor DHT!");
    delay(500);
  }
  //****** SENSOR DE PH  **************//
  float medicao = 2 * analogRead(measure); //medição so sensor
  double tensao = medicao * 5 / 4096; //conversão de leitura para tensão
  double ajuste = (4.3 - 3.40) / 3; //Ajuste da tensão [V(pH4) - V(pH7)]/(7-3)
  pH = 7 + ((3.40 - tensao) / ajuste); //pH = [(V(pH7)-tensao]/ajuste
  
  //****** SENSOR DE OXIGÊNIO  **************//
  oxigenio(); 
  //****** SENSOR DE OXIGÊNIO  **************//

   //****** Porcentagem bateria  **************//
   tensaoBat = (analogRead(pinBat)*3.3/4096)*2.79;
   porBat = (tensaoBat-6)*100/(2.4);
   //****** Porcentagem bateria  **************//
   

  
  
  // ENVIANDO PACOTES DA LORA
  LoRa.beginPacket(); //incia o pacote
  LoRa.print(pH);
  LoRa.print("q");
  LoRa.print(temp_agua);
  LoRa.print("q");
  LoRa.print(t);
  LoRa.print("q");
  LoRa.print(h);
  LoRa.print("q");
  LoRa.print(oxig);
  LoRa.print("q");
  LoRa.print(tensaoBat);
  LoRa.print("q");
  LoRa.print(porBat);
  LoRa.print("x");
  LoRa.endPacket(); //Termina os pacotes
  
  Serial.println("Pacotes enviados! ");
  Serial.println("-- VARIAVEIS --");
  Serial.println("pH: "+String(pH));
  Serial.print("Temp Agua: "+String(temp_agua));
  Serial.println(" ºC");
  Serial.print("Temperatura Ar: "+String(t));
  Serial.println(" ºC");
  Serial.print("Umidade Ar: "+String(h));
  Serial.println(" %");
  Serial.print("Oxigênio: "+String(oxig));
  Serial.println(" mg/L"); 
  Serial.print("Tensão Bateria: "+String(tensaoBat));
  Serial.println(" V"); 
  Serial.print("Porcentagem Bateria: "+String(porBat));
  Serial.println(" %"); 
//  display.clear();
 
  delay(1);
}

void oxigenio() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 30U)  //every 30 milliseconds,read the analog value from the ADC
  {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(DoSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
  }

  static unsigned long tempSampleTimepoint = millis();
  if (millis() - tempSampleTimepoint > 500U) // every 500 milliseconds, read the temperature
  {
    tempSampleTimepoint = millis();
    //temperature = readTemperature();  // add your temperature codes here to read the temperature, unit:^C
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 1000U)
  {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
    {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

    }
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0; // read the value more stable by the median filtering algorithm
    //Serial.print(F("Temperature:"));
    //Serial.print(temp_agua, 1);
    //Serial.print(F("^C"));
    doValue = pgm_read_float_near( &SaturationValueTab[0] + (int)(SaturationDoTemperature + 0.5) ) * averageVoltage / SaturationDoVoltage; //calculate the do value, doValue = Voltage / SaturationDoVoltage * SaturationDoValue(with temperature compensation)
    //Serial.print(F("DO Value:"));
    oxig = doValue*2;
    //Serial.print(doValue, 2);
    //Serial.println(F("mg/L"));
  }

  if (serialDataAvailable() > 0)
  {
    byte modeIndex = uartParse();  //parse the uart command received
    doCalibration(modeIndex);    // If the correct calibration command is received, the calibration function should be called.
  }
}


boolean serialDataAvailable(void)
{
  char receivedChar;
  static unsigned long receivedTimeOut = millis();
  while ( Serial.available() > 0 ) 
  {   
    if (millis() - receivedTimeOut > 500U) 
    {
      receivedBufferIndex = 0;
      memset(receivedBuffer,0,(ReceivedBufferLength+1));
    }
    receivedTimeOut = millis();
    receivedChar = Serial.read();
    if (receivedChar == '\n' || receivedBufferIndex == ReceivedBufferLength)
    {
  receivedBufferIndex = 0;
  strupr(receivedBuffer);
  return true;
    }else{
        receivedBuffer[receivedBufferIndex] = receivedChar;
        receivedBufferIndex++;
    }
  }
  return false;
}

byte uartParse()
{
    byte modeIndex = 0;
    if(strstr(receivedBuffer, "CALIBRATION") != NULL) 
        modeIndex = 1;
    else if(strstr(receivedBuffer, "EXIT") != NULL) 
        modeIndex = 3;
    else if(strstr(receivedBuffer, "SATCAL") != NULL)   
        modeIndex = 2;
    return modeIndex;
}

void doCalibration(byte mode)
{
    char *receivedBufferPtr;
    static boolean doCalibrationFinishFlag = 0,enterCalibrationFlag = 0;
    float voltageValueStore;
    switch(mode)
    {
      case 0:
      if(enterCalibrationFlag)
         Serial.println(F("Command Error"));
      break;
      
      case 1:
      enterCalibrationFlag = 1;
      doCalibrationFinishFlag = 0;
      Serial.println();
      Serial.println(F(">>>Enter Calibration Mode<<<"));
      Serial.println(F(">>>Please put the probe into the saturation oxygen water! <<<"));
      Serial.println();
      break;
     
     case 2:
      if(enterCalibrationFlag)
      {
         Serial.println();
         Serial.println(F(">>>Saturation Calibration Finish!<<<"));
         Serial.println();
         EEPROM_write(SaturationDoVoltageAddress, averageVoltage);
         EEPROM_write(SaturationDoTemperatureAddress, temp_agua);
         SaturationDoVoltage = averageVoltage;
         SaturationDoTemperature = temp_agua;
         doCalibrationFinishFlag = 1;
      }
      break;

        case 3:
        if(enterCalibrationFlag)
        {
            Serial.println();
            if(doCalibrationFinishFlag)      
               Serial.print(F(">>>Calibration Successful"));
            else 
              Serial.print(F(">>>Calibration Failed"));       
            Serial.println(F(",Exit Calibration Mode<<<"));
            Serial.println();
            doCalibrationFinishFlag = 0;
            enterCalibrationFlag = 0;
        }
        break;
    }
}

int getMedianNum(int bArray[], int iFilterLen) 
{
      int bTab[iFilterLen];
      for (byte i = 0; i<iFilterLen; i++)
      {
    bTab[i] = bArray[i];
      }
      int i, j, bTemp;
      for (j = 0; j < iFilterLen - 1; j++) 
      {
    for (i = 0; i < iFilterLen - j - 1; i++) 
          {
      if (bTab[i] > bTab[i + 1]) 
            {
    bTemp = bTab[i];
          bTab[i] = bTab[i + 1];
    bTab[i + 1] = bTemp;
       }
    }
      }
      if ((iFilterLen & 1) > 0)
  bTemp = bTab[(iFilterLen - 1) / 2];
      else
  bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
      return bTemp;
}

void readDoCharacteristicValues(void)
{
    EEPROM_read(SaturationDoVoltageAddress, SaturationDoVoltage);  
    EEPROM_read(SaturationDoTemperatureAddress, SaturationDoTemperature);
    if(EEPROM.read(SaturationDoVoltageAddress)==0xFF && EEPROM.read(SaturationDoVoltageAddress+1)==0xFF && EEPROM.read(SaturationDoVoltageAddress+2)==0xFF && EEPROM.read(SaturationDoVoltageAddress+3)==0xFF)
    {
      SaturationDoVoltage = 1127.6;   //default voltage:1127.6mv
      EEPROM_write(SaturationDoVoltageAddress, SaturationDoVoltage);
    }
    SaturationDoVoltage = 1127.6;   //default voltage:1127.6mv
      EEPROM_write(SaturationDoVoltageAddress, SaturationDoVoltage);
    if(EEPROM.read(SaturationDoTemperatureAddress)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+1)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+2)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+3)==0xFF)
    {
      SaturationDoTemperature = 25.0;   //default temperature is 25^C
      EEPROM_write(SaturationDoTemperatureAddress, SaturationDoTemperature);
    }  
    SaturationDoTemperature = 25.0;   //default temperature is 25^C
     EEPROM_write(SaturationDoTemperatureAddress, SaturationDoTemperature);
}
