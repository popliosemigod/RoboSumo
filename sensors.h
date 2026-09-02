// =====================================================================
//  ROBO SUMO - sensors.h
//  Olhos: 2x VL53L0X (Time-of-Flight, laser 940 nm) num barramento I2C
//  dedicado. Sensores IR de borda com ISR + confirmacao por ADC.
// =====================================================================
#pragma once
#include "config.h"
#include <Wire.h>
#include <VL53L0X.h>

namespace Sens {

// ---------------------------------------------------------------------
//  OLHOS - VL53L0X
//
//  Os dois modulos nascem com o mesmo endereco de fabrica (0x29 de 7
//  bits, 0x52 de 8 bits - datasheet §3). Para conviverem no mesmo
//  barramento, o boot liga um de cada vez pelo XSHUT e reendereça o
//  primeiro. XSHUT e ativo em nivel BAIXO e nunca pode ficar solto
//  (nota da §1.4: "must always be driven to avoid leakage current").
// ---------------------------------------------------------------------
VL53L0X tofL, tofR;
bool    haveL = false, haveR = false;

int16_t cmL = -1, cmR = -1;      // distancia publicada, em cm (-1 = nada)

// true quando so um dos dois olhos existe: nao da para triangular
inline bool caolho() { return haveL != haveR; }

// Presenca de verdade, nao "o init nao reclamou".
// Datasheet Tab. 4 (pg. 19): o registrador 0xC0 vale 0xEE e existe
// justamente "to validate the user I2C interface". Sem essa checagem o
// init() da biblioteca consegue "dar certo" falando com o vazio.
static bool tofResponde(uint8_t addr) {
  Wire1.beginTransmission(addr);
  Wire1.write((uint8_t)0xC0);
  if (Wire1.endTransmission() != 0) return false;
  if (Wire1.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return false;
  return (Wire1.read() == 0xEE);
}

static bool configura(VL53L0X& s) {
  s.setMeasurementTimingBudget(TOF_BUDGET_US);
  s.startContinuous(0);               // costas-com-costas, sem pausa
  return true;
}

static bool bringUp(VL53L0X& s, uint8_t xshut, uint8_t addr, const char* nome) {
  digitalWrite(xshut, HIGH);          // solta o reset deste sensor
  delay(10);                          // t_BOOT e 1,2 ms max (§2.9.1)

  if (!tofResponde(TOF_ADDR_R)) {
    Serial.printf("[tof] %s NAO respondeu (0xC0 != 0xEE)\n", nome);
    digitalWrite(xshut, LOW);         // desliga p/ nao atrapalhar o outro
    return false;
  }
  s.setBus(&Wire1);
  s.setTimeout(150);
  if (!s.init()) {
    Serial.printf("[tof] %s respondeu mas o init falhou\n", nome);
    digitalWrite(xshut, LOW);
    return false;
  }
  if (addr != TOF_ADDR_R) s.setAddress(addr);
  configura(s);
  Serial.printf("[tof] %s ok no endereco 0x%02X\n", nome, addr);
  return true;
}

bool wire1Pronto = false;

void tofBegin() {
  pinMode(PIN_TOF_L_XSHUT, OUTPUT);
  pinMode(PIN_TOF_R_XSHUT, OUTPUT);
  digitalWrite(PIN_TOF_L_XSHUT, LOW);     // os dois em reset
  digitalWrite(PIN_TOF_R_XSHUT, LOW);
  delay(20);

  if (!wire1Pronto) { Wire1.begin(PIN_TOF_SDA, PIN_TOF_SCL, 400000); wire1Pronto = true; }

  // Teste de XSHUT: com os dois em reset o barramento tem que estar VAZIO.
  // Se alguem responder aqui, aquele XSHUT nao esta sendo controlado.
  uint8_t vivos = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire1.beginTransmission(a);
    if (Wire1.endTransmission() == 0) {
      Serial.printf("[tof] 0x%02X responde com os dois XSHUT em reset\n", a);
      vivos++;
    }
  }

  if (vivos) {
    // Sem XSHUT nao da para separar os dois: eles nascem no mesmo 0x29 e
    // reendereçar so cria colisao silenciosa, com as duas medidas se
    // misturando no barramento. Melhor assumir UM olho e avisar alto.
    Serial.println("[tof] ================================================");
    Serial.println("[tof] XSHUT NAO ESTA LIGADO - so da para usar um olho");
    Serial.println("[tof] ligue  XSHUT esquerdo -> GPIO 19");
    Serial.println("[tof]        XSHUT direito  -> GPIO 18");
    Serial.println("[tof] sem isso os dois ficam no mesmo endereco e as");
    Serial.println("[tof] leituras se misturam - nao da para triangular.");
    Serial.println("[tof] ================================================");
    digitalWrite(PIN_TOF_L_XSHUT, HIGH);
    digitalWrite(PIN_TOF_R_XSHUT, HIGH);
    delay(10);
    uint8_t addr = tofResponde(TOF_ADDR_R) ? TOF_ADDR_R : TOF_ADDR_L;
    tofL.setBus(&Wire1);
    tofL.setTimeout(150);
    tofL.setAddress(addr);
    haveL = tofL.init();
    if (haveL) { configura(tofL); Serial.printf("[tof] um olho ativo em 0x%02X\n", addr); }
    haveR = false;
  } else {
    Serial.println("[tof] XSHUT dos dois ok (barramento vazio em reset)");
    // esquerdo primeiro ganha endereco novo; o direito fica no padrao
    haveL = bringUp(tofL, PIN_TOF_L_XSHUT, TOF_ADDR_L, "olho ESQUERDO");
    haveR = bringUp(tofR, PIN_TOF_R_XSHUT, TOF_ADDR_R, "olho DIREITO");
  }
  T.tofOkL = haveL; T.tofOkR = haveR;

  if (haveL && haveR)      Serial.println("[tof] dois olhos: mira por triangulacao");
  else if (haveL || haveR) Serial.println("[tof] MODO CAOLHO: um olho so, avanca reto no alvo");
  else                     Serial.println("[tof] NENHUM olho respondeu - confira SDA/SCL/VIN");
}

// Converte uma leitura crua para cm, ou -1 se nao vale.
// A biblioteca devolve 65535 no timeout; o proprio sensor devolve
// ~8190 mm quando nao encontra alvo nenhum.
static int16_t toCm(VL53L0X& s, uint16_t mm, bool& ok) {
  ok = !(s.timeoutOccurred() || mm == 0 || mm >= 8000);
  if (!ok) return -1;
  int16_t cm = (int16_t)((mm + 5) / 10);
  if (cm > (int16_t)P.rangeCm) return -1;      // fora do alcance util
  return cm;
}

void tofTask(void*) {
  uint32_t ultimaSonda = 0;
  for (;;) {
    // Bancada: se nenhum olho subiu no boot, fica sondando o barramento e
    // religa sozinho quando o sensor aparecer. Evita ter que resetar a
    // placa a cada fio que se mexe.
    if (!haveL && !haveR) {
      if (millis() - ultimaSonda > 1500) {
        ultimaSonda = millis();
        digitalWrite(PIN_TOF_L_XSHUT, HIGH);      // acorda quem estiver ai
        digitalWrite(PIN_TOF_R_XSHUT, HIGH);
        delay(5);
        bool algo = tofResponde(TOF_ADDR_R) || tofResponde(TOF_ADDR_L);
        T.tofNoBus = algo;
        if (algo) {
          Serial.println("[tof] sensor apareceu no barramento - reinicializando");
          tofBegin();
        } else {
          digitalWrite(PIN_TOF_L_XSHUT, LOW);
          digitalWrite(PIN_TOF_R_XSHUT, LOW);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    T.tofNoBus = true;

    if (haveL) {
      uint16_t mm = tofL.readRangeContinuousMillimeters();
      bool ok; int16_t cm = toCm(tofL, mm, ok);
      T.tofRawL = ok ? (int16_t)mm : -1;
      if (ok) T.tofFailL = 0; else if (T.tofFailL < 60000) T.tofFailL++;
      cmL = cm; T.distL = cm;
    }
    vTaskDelay(pdMS_TO_TICKS(4));

    if (haveR) {
      uint16_t mm = tofR.readRangeContinuousMillimeters();
      bool ok; int16_t cm = toCm(tofR, mm, ok);
      T.tofRawR = ok ? (int16_t)mm : -1;
      if (ok) T.tofFailR = 0; else if (T.tofFailR < 60000) T.tofFailR++;
      cmR = cm; T.distR = cm;
    }
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

// ---------------------------------------------------------------------
//  IR DE BORDA
//  DO -> interrupcao (reacao em microssegundos)
//  AO -> ADC1 (usado para calibrar o limiar pelo painel web)
// ---------------------------------------------------------------------
SemaphoreHandle_t edgeSem = nullptr;
volatile bool     edgeL = false, edgeR = false;
volatile uint32_t edgeStampUs = 0;

static inline bool rawEdge(bool pinLevel) {
  return P.irActiveLow ? (pinLevel == LOW) : (pinLevel == HIGH);
}

void IRAM_ATTR isrIrL() {
  bool v = rawEdge(digitalRead(PIN_IR_L_D));
  edgeL = v;
  if (v) {
    edgeStampUs = micros();
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(edgeSem, &hp);
    if (hp) portYIELD_FROM_ISR();
  }
}
void IRAM_ATTR isrIrR() {
  bool v = rawEdge(digitalRead(PIN_IR_R_D));
  edgeR = v;
  if (v) {
    edgeStampUs = micros();
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(edgeSem, &hp);
    if (hp) portYIELD_FROM_ISR();
  }
}

// Ressincroniza o estado da borda a cada ciclo (~200 Hz).
//
// A interrupcao no pino DO continua sendo o caminho rapido, mas ela so
// dispara em transicao: se o robo ligar ja em cima da faixa branca, nenhuma
// transicao acontece. Por isso relemos o pino aqui tambem.
//
// O caminho analogico NAO sobrepoe o digital - ele soma. Enquanto o
// limiar nao estiver calibrado (irUseAnalog=0), um AO desconectado lendo 0
// nao consegue travar o robo em manobra de borda eterna.
void pollIrAnalog() {
  uint16_t a = analogRead(PIN_IR_L_A);
  uint16_t b = analogRead(PIN_IR_R_A);
  T.irLraw = a; T.irRraw = b;

  bool dl = rawEdge(digitalRead(PIN_IR_L_D));
  bool dr = rawEdge(digitalRead(PIN_IR_R_D));
  T.irLdo = dl; T.irRdo = dr;

  bool al = P.irActiveLow ? (a < P.irThreshold) : (a > P.irThreshold);
  bool ar = P.irActiveLow ? (b < P.irThreshold) : (b > P.irThreshold);

  bool l, r;
  switch (P.irSource) {
    case 0:  l = dl;       r = dr;       break;   // so o pino digital
    case 2:  l = dl || al; r = dr || ar; break;   // qualquer um dos dois
    default: l = al;       r = ar;       break;   // so o ADC (padrao)
  }

  if (l && !edgeL) xSemaphoreGive(edgeSem);
  if (r && !edgeR) xSemaphoreGive(edgeSem);
  edgeL = l; edgeR = r;
  T.irL = l; T.irR = r;
}

// ---------------------------------------------------------------------
//  BATERIA
// ---------------------------------------------------------------------
float vbatFilt = 0;
void pollVbat() {
  // divisor 100k / 47k  ->  Vbat = Vadc * (147/47) = Vadc * 3.128
  uint32_t acc = 0;
  for (int i = 0; i < 4; i++) acc += analogRead(PIN_VBAT);
  float v = (acc / 4.0f) * 3.3f / 4095.0f * 3.128f;
  vbatFilt = vbatFilt ? (vbatFilt * 0.9f + v * 0.1f) : v;
  T.vbat = (uint16_t)(vbatFilt * 100.0f);
}

// ---------------------------------------------------------------------
//  AUTOTESTE (roda uma vez no boot)
// ---------------------------------------------------------------------
static void scanBus(TwoWire& bus, const char* nome) {
  Serial.printf("[teste] varrendo %s:", nome);
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) { Serial.printf(" 0x%02X", a); n++; }
  }
  if (!n) Serial.print("  NENHUM DISPOSITIVO");
  Serial.println();
}

void selfTest() {
  Serial.println("[teste] --- autoteste ---");
  scanBus(Wire,  "I2C do OLED (SDA 21 / SCL 22)");
  scanBus(Wire1, "I2C dos olhos (SDA 16 / SCL 17)");
  Serial.printf("[teste] IR DO esq (GPIO34): %s | IR DO dir (GPIO35): %s\n",
                digitalRead(PIN_IR_L_D) ? "ALTO" : "BAIXO",
                digitalRead(PIN_IR_R_D) ? "ALTO" : "BAIXO");
  Serial.printf("[teste] AO esq (GPIO32)=%u  AO dir (GPIO33)=%u  VBAT (GPIO39)=%u de 4095\n",
                analogRead(PIN_IR_L_A), analogRead(PIN_IR_R_A), analogRead(PIN_VBAT));
  Serial.println("[teste] --- fim ---");
}

// ---------------------------------------------------------------------
void begin() {
  pinMode(PIN_IR_L_D, INPUT);
  pinMode(PIN_IR_R_D, INPUT);
  edgeSem = xSemaphoreCreateBinary();
  attachInterrupt(digitalPinToInterrupt(PIN_IR_L_D), isrIrL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IR_R_D), isrIrR, CHANGE);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_IR_L_A, ADC_11db);
  analogSetPinAttenuation(PIN_IR_R_A, ADC_11db);
  analogSetPinAttenuation(PIN_VBAT,   ADC_11db);

  tofBegin();
}

} // namespace Sens
