/*

  - Datalogger Agri-PV
  - 10 sensores de radiação (placa 300x190 - 1.23 volts / 1000 watts)
  - 4 pluviometros (ReedSwitch)
  - 4 termo-higrômetros (DHT22)

*/


#include <SimpleDHT.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

#define MUX_SIGNAL_PIN 34  // ADC do ESP32

#define S0 16
#define S1 17
#define S2 2
#define S3 4

const int chipSelect = 5;

float delay_mensura = 0;

const int pinDHT1 = 13;
const int pinDHT2 = 25;
const int pinDHT3 = 26;
const int pinDHT4 = 27;

const int pinPluv1 = 32;
const int pinPluv2 = 33;
const int pinPluv3 = 35;
const int pinPluv4 = 39;

char timeStringBuff[21];

int i = 0;
int dia = 0;
int mes = 0;
int ano = 0;
int hora = 0;
int minuto = 0;
int segundo = 0;


float ppt[4] = { 0, 0, 0, 0 };
float t[4] = { 0 }, h[4] = { 0 };  // Médias acumuladas
float t_inst[4], h_inst[4];        // Valores instantâneos para o Serial

float r_inst[10] = { 0 };  // r1 a r10
float r_acum[10] = { 0 };  // r_1 a r_10

volatile long pulseCount1 = 0;
volatile long pulseCount2 = 0;
volatile long pulseCount3 = 0;
volatile long pulseCount4 = 0;

volatile unsigned long lastPulseTime1 = 0;
volatile unsigned long lastPulseTime2 = 0;
volatile unsigned long lastPulseTime3 = 0;
volatile unsigned long lastPulseTime4 = 0;
const unsigned long debounceDelay = 500;  // 500 milissegundos

RTC_DS3231 rtc;

SimpleDHT22 dht1(pinDHT1);
SimpleDHT22 dht2(pinDHT2);
SimpleDHT22 dht3(pinDHT3);
SimpleDHT22 dht4(pinDHT4);


void mensura();
void gera_dado();
void data_hora();
void salva_dado();
void multiplexador();


void IRAM_ATTR countPulse1() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime1 > debounceDelay) {
    pulseCount1++;
    lastPulseTime1 = currentTime;
  }
}

void IRAM_ATTR countPulse2() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime2 > debounceDelay) {
    pulseCount2++;
    lastPulseTime2 = currentTime;
  }
}

void IRAM_ATTR countPulse3() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime3 > debounceDelay) {
    pulseCount3++;
    lastPulseTime3 = currentTime;
  }
}

void IRAM_ATTR countPulse4() {
  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime4 > debounceDelay) {
    pulseCount4++;
    lastPulseTime4 = currentTime;
  }
}

void setup() {

  Serial.begin(9600);
  rtc.begin();

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  pinMode(MUX_SIGNAL_PIN, INPUT);

  if (!SD.begin(chipSelect)) {
    Serial.println("Erro ao inicializar SD Card!");
    while (1)
      ;  // trava o programa
  }
  Serial.println("SD Card inicializado.");

  // Cria arquivo se não existir
  if (!SD.exists("/dados.csv")) {
    File file = SD.open("/dados.csv", FILE_APPEND);
    if (file) {
      // Substitua a linha do cabeçalho por esta:
      file.println("Timestamp;T1;T2;T3;T4;H1;H2;H3;H4;PPT1;PPT2;PPT3;PPT4;R1;R2;R3;R4;R5;R6;R7;R8;R9;R10");
      file.close();
    }
  }

  pinMode(pinPluv1, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinPluv1), countPulse1, RISING);

  pinMode(pinPluv2, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinPluv2), countPulse2, RISING);

  pinMode(pinPluv3, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinPluv3), countPulse3, RISING);

  pinMode(pinPluv4, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinPluv4), countPulse4, RISING);

  delay_mensura = millis();
}

void loop() {

  data_hora();

  if (millis() - delay_mensura > 10000) mensura();
  if (minuto % 10 == 0 && i > 10) gera_dado();

  delay(50);
}

void mensura() {

  i++;
  delay_mensura = millis();

  multiplexador();

  noInterrupts();
  ppt[0] += 0.25 * pulseCount1;
  pulseCount1 = 0;
  ppt[1] += 0.25 * pulseCount2;
  pulseCount2 = 0;
  ppt[2] += 0.25 * pulseCount3;
  pulseCount3 = 0;
  ppt[3] += 0.25 * pulseCount4;
  pulseCount4 = 0;
  interrupts();

  Serial.println(timeStringBuff);  // printar data e hora

  SimpleDHT22* dhts[] = { &dht1, &dht2, &dht3, &dht4 };
  for (int j = 0; j < 4; j++) {
    float temp = 0, hum = 0;
    dhts[j]->read2(&temp, &hum, NULL);

    t_inst[j] = temp;
    h_inst[j] = hum;  // Para o Serial
    t[j] += temp;
    h[j] += hum;  // Acumula para média

    Serial.printf("Sensor_%d -> Temp: %.1f | UR: %.1f | PPT: %.2f\n", j + 1, temp, hum, ppt[j]);
  }

  // Radiação (Simplificado com loop)
  for (int j = 0; j < 10; j++) {
    Serial.printf("Rad_%d: %.0f  ", j + 1, r_inst[j]);
    if ((j + 1) % 3 == 0) Serial.println();  // Quebra linha a cada 3
  }
  Serial.println();
}

void gera_dado() {
  // Calcula as médias usando loops
  for (int j = 0; j < 4; j++) {
    t[j] /= i;
    h[j] /= i;
  }
  for (int j = 0; j < 10; j++) {
    r_acum[j] /= i;
  }

  salva_dado();

  // Reseta tudo para o próximo ciclo
  i = 0;
  for (int j = 0; j < 4; j++) {
    t[j] = 0;
    h[j] = 0;
    ppt[j] = 0;
  }
  for (int j = 0; j < 10; j++) {
    r_acum[j] = 0;
  }
}

void salva_dado() {
  File file = SD.open("/dados.csv", FILE_APPEND);
  if (!file) return;

  file.print(timeStringBuff);
  file.print(";");

  // Salva T, H e PPT para os 4 sensores
  for (int j = 0; j < 4; j++) {
    file.printf("%.2f;%.2f;%.2f;", t[j], h[j], ppt[j]);
  }

  // Salva os 10 sensores de Radiação
  for (int j = 0; j < 10; j++) {
    file.printf("%.2f%s", r_acum[j], (j == 9) ? "" : ";");
  }

  file.println();  // Pula linha para o próximo registro
  file.close();
  Serial.println(">>> MEDIAS SALVAS NO SD <<<");
}

void data_hora() {

  DateTime now = rtc.now();
  // Formata a string de data e hora
  sprintf(timeStringBuff, "%02d/%02d/%d  %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
  dia = now.day();
  mes = now.month();
  ano = now.year();
  hora = now.hour();
  minuto = now.minute();
  segundo = now.second();
}

void multiplexador() {
  // Fator de conversão: (3.3V / 4095) * (1000W / 1.23V)
  const float fatorConversao = 0.65516;

  for (int canal = 0; canal < 10; canal++) {
    // 1. Seleciona o canal no multiplexador via lógica binária
    digitalWrite(S0, bitRead(canal, 0));
    digitalWrite(S1, bitRead(canal, 1));
    digitalWrite(S2, bitRead(canal, 2));
    digitalWrite(S3, bitRead(canal, 3));

    // 2. Aguarda a estabilização da tensão no pino (crucial para precisão)
    delayMicroseconds(20);

    // 3. Lê o valor bruto, converte e salva no array de valores instantâneos
    r_inst[canal] = analogRead(MUX_SIGNAL_PIN) * fatorConversao;

    // 4. Acumula o valor para o cálculo da média posterior
    r_acum[canal] += r_inst[canal];
  }
}