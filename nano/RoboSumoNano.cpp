// =====================================================================
//  ROBO SUMO - porte bruto para Arduino Nano (ATmega328P)
//
//  Isto e uma REGRESSAO deliberada do firmware ESP32, para isolar se os
//  problemas de bancada eram do controlador ou da fiacao. O que se perde
//  em relacao ao original, e que nao da para recuperar neste chip:
//
//    - Wi-Fi, painel web e OTA. Nao existem. Toda a operacao passa a ser
//      pela serial a 115200, que aqui vira a unica interface.
//    - Dois nucleos e FreeRTOS. Aqui e um loop so. A guarda de borda
//      deixa de ser uma task de prioridade 6 e vira interrupcao + um
//      teste no topo do loop; por isso NADA no loop pode bloquear.
//    - Dois barramentos I2C. O OLED divide A4/A5 com os olhos, entao o
//      desenho do display e suspenso durante ataque e borda.
//    - PWM de 20 kHz. Vira 976 Hz (esquerda) e 490 Hz (direita), porque
//      os timers do AVR sao compartilhados. Motor passa a assobiar.
//
//  O que se mantem: pinout coerente, guarda de borda por interrupcao,
//  reendereçamento dos dois VL53L0X por XSHUT, PID de direcionamento,
//  contagem regulamentar e os instrumentos de bancada na serial.
// =====================================================================
// Arquivo .cpp e nao .ino de proposito: com src_dir = "." o PlatformIO
// funde TODOS os .ino da arvore num unico sketch, e o do ESP32 na raiz
// entraria junto. Em .cpp cada ambiente compila so a sua pasta.
#include <Arduino.h>
#include "pinos.h"
#include <Wire.h>
#include <VL53L0X.h>

// DESLIGADO POR PADRAO, e a razao e medida, nao gosto:
//
//   com OLED : flash 28540/30720 (92,9%), RAM estatica 711 B
//   sem OLED : flash 17588/30720 (57,3%), RAM estatica 588 B
//
// O buffer do SSD1306 sao mais 1024 B alocados em tempo de execucao e
// que NAO aparecem no numero de RAM estatica. Com o display ligado
// sobram ~313 bytes para a pilha - e a pilha aqui carrega chamadas da
// biblioteca do VL53L0X, printf de float e quadros de interrupcao.
// Estourar isso nao da erro: da reset aleatorio, que e exatamente o
// sintoma que este porte existe para NAO ter enquanto diagnostica.
//
// Ligue em 1 se quiser a carinha e aceitar o risco; a serial ja mostra
// tudo o que o display mostraria.
#define USAR_OLED 0

#if USAR_OLED
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 oled(128, 64, &Wire, -1);
  bool oledOk = false;
#endif

// ---------------------------------------------------------------------
//  PARAMETROS  (equivalentes aos do ESP32, em escala de 8 bits)
// ---------------------------------------------------------------------
struct Params {
  uint8_t  vSearch, vAttack, vMax, vReverse;
  uint8_t  motInvL, motInvR;
  float    kp, ki, kd;
  uint8_t  rangeCm, confirmHits, loseMisses, jumpCm;
  uint16_t edgeBackMs, edgeTurnMs, sweepMs, stuckMs, countdownMs;
  uint16_t irThreshold;
  uint8_t  irSource;       // 0 = so DO | 1 = so AO | 2 = qualquer um
  uint8_t  irActiveLow;
  uint8_t  mode, soundOn, faceOn;
  uint16_t vbatMin;        // centesimos de volt
};

Params P = {
  /*vSearch*/107, /*vAttack*/230, /*vMax*/209, /*vReverse*/191,
  /*motInv*/0, 0,
  /*kp*/6.0f, /*ki*/0.05f, /*kd*/2.2f,
  /*rangeCm*/80, /*confirmHits*/3, /*loseMisses*/6, /*jumpCm*/35,
  /*edgeBackMs*/260, /*edgeTurnMs*/230, /*sweepMs*/900,
  /*stuckMs*/1400, /*countdownMs*/5000,
  // irSource = 0 (SO o pino digital) porque o modulo em uso tem tres
  // fios - VCC, GND e OUT - e nao possui saida analogica. Nao ha o que
  // ler em A0/A1. O limiar deixa de ser deste parametro e passa a ser o
  // trimpot do proprio modulo; irThreshold fica sem efeito com irSource=0.
  /*irThreshold*/512, /*irSource*/0, /*irActiveLow*/1,
  /*mode*/MODE_NORMAL, /*soundOn*/1, /*faceOn*/1, /*vbatMin*/660
};

// ---------------------------------------------------------------------
//  TELEMETRIA
// ---------------------------------------------------------------------
struct Telemetria {
  uint8_t  state, mode;
  bool     armed;
  int16_t  distL, distR;
  int16_t  irLraw, irRraw;
  bool     irLdo, irRdo, irL, irR;
  int16_t  tofRawL, tofRawR;
  bool     tofOkL, tofOkR;
  int16_t  pwmL, pwmR;
  uint16_t vbat;
  bool     drvFault;
  uint16_t loopHz;
  uint16_t nAttacks, nEdgeSaves, nLost;
};
Telemetria T;

// ---------------------------------------------------------------------
//  MOTORES
//
//  Decaimento lento (Tab. 3 do DRV8833): um pino em ALTO e o outro
//  recebendo o PWM invertido. Da mais torque em baixa velocidade que o
//  decaimento rapido, que e o que interessa empurrando.
// ---------------------------------------------------------------------
namespace Mot {

bool acordado = false;

void begin() {
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  pinMode(PIN_DRV_FAULT, INPUT_PULLUP);
  digitalWrite(PIN_DRV_SLEEP, LOW);          // nasce dormindo
  analogWrite(PIN_IN1, 0); analogWrite(PIN_IN2, 0);
  analogWrite(PIN_IN3, 0); analogWrite(PIN_IN4, 0);
}

void wake(bool on) { acordado = on; digitalWrite(PIN_DRV_SLEEP, on ? HIGH : LOW); }

static void lado(uint8_t pinA, uint8_t pinB, int16_t v, bool inverter) {
  if (inverter) v = -v;
  v = clampi(v, -PWM_MAX, PWM_MAX);
  if (v > 0)      { digitalWrite(pinA, HIGH); analogWrite(pinB, PWM_MAX - v); }
  else if (v < 0) { digitalWrite(pinB, HIGH); analogWrite(pinA, PWM_MAX + v); }
  else            { digitalWrite(pinA, LOW);  digitalWrite(pinB, LOW); }  // coast
}

void aplica(int16_t esq, int16_t dir) {
  esq = clampi(esq, -(int16_t)P.vMax, (int16_t)P.vMax);
  dir = clampi(dir, -(int16_t)P.vMax, (int16_t)P.vMax);
  T.pwmL = esq; T.pwmR = dir;
  lado(PIN_IN1, PIN_IN2, esq, P.motInvL);
  lado(PIN_IN3, PIN_IN4, dir, P.motInvR);
}

// Freio eletrico: os dois pinos do lado em ALTO curto-circuitam o motor.
void freio() {
  digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, HIGH);
  T.pwmL = T.pwmR = 0;
}

} // namespace Mot

// ---------------------------------------------------------------------
//  BUZZER - nao bloqueante, no Timer2 (tone)
// ---------------------------------------------------------------------
namespace Snd {

uint32_t fim = 0;
bool tocando = false;

void beep(uint16_t hz, uint16_t ms) {
  if (!P.soundOn) return;
  tone(PIN_BUZZER, hz);
  fim = millis() + ms;
  tocando = true;
}

void update() {
  if (tocando && (int32_t)(millis() - fim) >= 0) { noTone(PIN_BUZZER); tocando = false; }
}

} // namespace Snd

// ---------------------------------------------------------------------
//  IR DE BORDA
//
//  A interrupcao so levanta a bandeira; quem decide e o loop. Isso e de
//  proposito: dentro de ISR nao da para usar I2C nem Serial, e a acao de
//  borda precisa dos dois.
// ---------------------------------------------------------------------
volatile bool bordaFlag = false;

static inline bool bordaCrua(bool nivel) {
  return P.irActiveLow ? (nivel == LOW) : (nivel == HIGH);
}

ISR(INT0_vect)  { bordaFlag = true; }   // D2 - IR esquerdo
ISR(PCINT2_vect){ bordaFlag = true; }   // D4 - IR direito

namespace Sens {

VL53L0X tofL, tofR;
bool haveL = false, haveR = false;

// O registrador 0xC0 vale 0xEE em todo VL53L0X (Tab. 4). E assim que se
// separa "sensor de verdade" de "barramento vazio" - sem isso o init()
// da biblioteca consegue dar certo falando com o nada.
static bool tofResponde(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)0xC0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  return (Wire.read() == 0xEE);
}

static bool sobe(VL53L0X& s, uint8_t xshut, uint8_t addr, const __FlashStringHelper* nome) {
  digitalWrite(xshut, HIGH);
  delay(10);                                  // t_BOOT e 1,2 ms max
  if (!tofResponde(TOF_ADDR_R)) {
    Serial.print(F("[tof] ")); Serial.print(nome);
    Serial.println(F(" NAO respondeu (0xC0 != 0xEE)"));
    digitalWrite(xshut, LOW);
    return false;
  }
  s.setTimeout(150);
  if (!s.init()) {
    Serial.print(F("[tof] ")); Serial.print(nome); Serial.println(F(" init falhou"));
    digitalWrite(xshut, LOW);
    return false;
  }
  if (addr != TOF_ADDR_R) s.setAddress(addr);
  s.setMeasurementTimingBudget(TOF_BUDGET_US);
  s.startContinuous(0);
  Serial.print(F("[tof] ")); Serial.print(nome);
  Serial.print(F(" ok em 0x")); Serial.println(addr, HEX);
  return true;
}

void tofBegin() {
  pinMode(PIN_TOF_L_XSHUT, OUTPUT);
  pinMode(PIN_TOF_R_XSHUT, OUTPUT);
  digitalWrite(PIN_TOF_L_XSHUT, LOW);
  digitalWrite(PIN_TOF_R_XSHUT, LOW);
  delay(20);

  // Com os dois em reset o barramento nao pode ter nenhum 0x29. Se tiver,
  // aquele XSHUT nao esta chegando no modulo.
  if (tofResponde(TOF_ADDR_R)) {
    Serial.println(F("[tof] alguem responde com XSHUT em BAIXO - XSHUT nao ligado"));
  }

  haveL = sobe(tofL, PIN_TOF_L_XSHUT, TOF_ADDR_L, F("olho ESQUERDO"));
  haveR = sobe(tofR, PIN_TOF_R_XSHUT, TOF_ADDR_R, F("olho DIREITO"));
  T.tofOkL = haveL; T.tofOkR = haveR;

  if (haveL && haveR)      Serial.println(F("[tof] dois olhos: mira por triangulacao"));
  else if (haveL || haveR) Serial.println(F("[tof] MODO CAOLHO: avanca reto no alvo"));
  else                     Serial.println(F("[tof] NENHUM olho - confira SDA/SCL/VIN"));
}

static int16_t paraCm(VL53L0X& s, uint16_t mm) {
  if (s.timeoutOccurred() || mm == 0 || mm >= 8000) return -1;
  int16_t cm = (int16_t)((mm + 5) / 10);
  return (cm > (int16_t)P.rangeCm) ? -1 : cm;
}

// Le UM olho por chamada, alternando. Ler os dois de uma vez custaria
// ~50 ms de loop e o robo ficaria cego para a borda nesse intervalo.
void tofPoll() {
  static bool vezDoEsquerdo = true;
  if (vezDoEsquerdo && haveL) {
    uint16_t mm = tofL.readRangeContinuousMillimeters();
    int16_t cm = paraCm(tofL, mm);
    T.tofRawL = (cm < 0) ? -1 : (int16_t)mm;
    T.distL = cm;
  } else if (!vezDoEsquerdo && haveR) {
    uint16_t mm = tofR.readRangeContinuousMillimeters();
    int16_t cm = paraCm(tofR, mm);
    T.tofRawR = (cm < 0) ? -1 : (int16_t)mm;
    T.distR = cm;
  }
  vezDoEsquerdo = !vezDoEsquerdo;
}

void irPoll() {
  // So le o analogico se alguem pediu por ele. Com modulo de tres fios
  // (VCC/GND/OUT) A0 e A1 ficam soltos, e ler pino solto a 200 Hz gasta
  // tempo para produzir numero inventado - que foi exatamente o que
  // atrapalhou o diagnostico na placa anterior.
  uint16_t a = 0, b = 0;
  if (P.irSource != 0) {
    a = analogRead(PIN_IR_L_A);
    b = analogRead(PIN_IR_R_A);
  }
  T.irLraw = a; T.irRraw = b;

  bool dl = bordaCrua(digitalRead(PIN_IR_L_D));
  bool dr = bordaCrua(digitalRead(PIN_IR_R_D));
  T.irLdo = dl; T.irRdo = dr;

  bool al = P.irActiveLow ? (a < P.irThreshold) : (a > P.irThreshold);
  bool ar = P.irActiveLow ? (b < P.irThreshold) : (b > P.irThreshold);

  switch (P.irSource) {
    case 0:  T.irL = dl;       T.irR = dr;       break;
    case 2:  T.irL = dl || al; T.irR = dr || ar; break;
    default: T.irL = al;       T.irR = ar;       break;
  }
}

void vbatPoll() {
  // divisor 100k/47k -> Vbat = Vadc * 3,128 ; ADC de 10 bits, ref 5 V
  static float filt = 0;
  float v = analogRead(PIN_VBAT) * 5.0f / 1023.0f * 3.128f;
  filt = filt ? (filt * 0.9f + v * 0.1f) : v;
  T.vbat = (uint16_t)(filt * 100.0f);
}

void begin() {
  pinMode(PIN_IR_L_D, INPUT_PULLUP);
  pinMode(PIN_IR_R_D, INPUT_PULLUP);

  // INT0 (D2) em qualquer mudanca
  EICRA |= (1 << ISC00); EICRA &= ~(1 << ISC01);
  EIMSK |= (1 << INT0);
  // PCINT20 (D4)
  PCICR  |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20);

  tofBegin();
}

} // namespace Sens

// ---------------------------------------------------------------------
//  AUTOTESTE E INSTRUMENTOS DE BANCADA
// ---------------------------------------------------------------------
static uint8_t varreI2C() {
  Serial.print(F("[teste] varrendo I2C (A4/A5):"));
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F(" 0x")); Serial.print(a, HEX); n++;
    }
  }
  if (!n) Serial.print(F("  NENHUM DISPOSITIVO"));
  Serial.println();
  return n;
}

// Sonda eletrica: descarrega a linha e ve quem a levanta. Pull-up externo
// levanta em microssegundos; linha aberta fica em baixo. Sem descarregar
// antes, a capacitancia da propria linha segura o nivel alto e uma linha
// solta daria o mesmo resultado de uma com pull-up.
static void sondaLinha(uint8_t pino, const __FlashStringHelper* nome) {
  uint8_t solto = 0, comPull = 0;
  pinMode(pino, OUTPUT); digitalWrite(pino, LOW); delayMicroseconds(1500);
  pinMode(pino, INPUT);  delayMicroseconds(80);
  for (uint8_t i = 0; i < 32; i++) { if (digitalRead(pino)) solto++; delayMicroseconds(120); }
  pinMode(pino, OUTPUT); digitalWrite(pino, LOW); delayMicroseconds(1500);
  pinMode(pino, INPUT_PULLUP); delayMicroseconds(80);
  for (uint8_t i = 0; i < 32; i++) { if (digitalRead(pino)) comPull++; delayMicroseconds(120); }
  pinMode(pino, INPUT);

  Serial.print(F("[teste]   ")); Serial.print(nome);
  Serial.print(F(" solto=")); Serial.print(solto);
  Serial.print(F("/32 comPull=")); Serial.print(comPull);
  if (solto >= 29)        Serial.println(F(" -> alguem segura em ALTO (pull-up, ou fio no VCC)"));
  else if (comPull >= 29) Serial.println(F(" -> linha ABERTA, nada ligado"));
  else                    Serial.println(F(" -> presa em BAIXO, curto ou escravo travado"));
}

void autoteste() {
  Serial.println(F("[teste] --- autoteste ---"));
  uint8_t n = varreI2C();
  if (!n) {
    Wire.end();
    sondaLinha(A4, F("SDA (A4)"));
    sondaLinha(A5, F("SCL (A5)"));
    Wire.begin();
  }
  // Controle negativo: A6/A7 nao sao usados neste projeto. Se o pino
  // livre acusar pull-up, a sonda esta mentindo e nada dela vale.
  Serial.print(F("[teste] IR DO esq(D2)=")); Serial.print(digitalRead(PIN_IR_L_D) ? F("ALTO") : F("BAIXO"));
  Serial.print(F(" | IR DO dir(D4)="));      Serial.println(digitalRead(PIN_IR_R_D) ? F("ALTO") : F("BAIXO"));
  if (P.irSource != 0) {
    Serial.print(F("[teste] AO esq(A0)="));  Serial.print(analogRead(PIN_IR_L_A));
    Serial.print(F("  AO dir(A1)="));        Serial.println(analogRead(PIN_IR_R_A));
  } else {
    Serial.println(F("[teste] AO nao usado: modulo de 3 fios, limiar e o trimpot"));
  }
  Serial.print(F("[teste] VBAT(A2)="));      Serial.print(analogRead(PIN_VBAT));
  Serial.println(F(" de 1023"));
  Serial.print(F("[teste] nFAULT(D8)="));    Serial.println(digitalRead(PIN_DRV_FAULT) ? F("ok") : F("FALHA ATIVA"));
  Serial.println(F("[teste] --- fim ---"));
}

// Exame dos olhos: varre com XSHUT baixo/alto/solto para separar as
// causas que a varredura simples nao separa.
void exameOlhos() {
  Serial.println(F("[exame] --- exame dos olhos ---"));
  pinMode(PIN_TOF_L_XSHUT, OUTPUT); pinMode(PIN_TOF_R_XSHUT, OUTPUT);

  digitalWrite(PIN_TOF_L_XSHUT, LOW); digitalWrite(PIN_TOF_R_XSHUT, LOW);
  delay(25); Serial.print(F("[exame] XSHUT BAIXO (esperado vazio):")); varreI2C();

  digitalWrite(PIN_TOF_L_XSHUT, HIGH); digitalWrite(PIN_TOF_R_XSHUT, HIGH);
  delay(25); Serial.print(F("[exame] XSHUT ALTO (esperado 0x29):")); varreI2C();

  pinMode(PIN_TOF_L_XSHUT, INPUT); pinMode(PIN_TOF_R_XSHUT, INPUT);
  delay(25); Serial.print(F("[exame] XSHUT SOLTO (pull-up do modulo):")); varreI2C();

  Serial.println(F("[exame] --- fim; reinicializando ---"));
  Sens::tofBegin();
}

// ---------------------------------------------------------------------
//  CARINHA - minima, e suspensa quando o robo esta lutando
//
//  Aqui o display divide o barramento com os olhos. Um quadro custa
//  ~25 ms de linha ocupada; durante ATAQUE e BORDA isso seria 25 ms sem
//  saber a distancia, entao simplesmente nao se desenha nesses estados.
// ---------------------------------------------------------------------
#if USAR_OLED
namespace Face {

void begin() {
  Wire.beginTransmission(0x3C);
  bool achou = (Wire.endTransmission() == 0);
  oledOk = achou && oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  if (!oledOk) { Serial.println(F("[oled] NAO encontrado em 0x3C")); return; }
  Serial.println(F("[oled] SSD1306 ok em 0x3C"));
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2); oled.setCursor(4, 20); oled.print(F("ROBO SUMO"));
  oled.display();
}

void render() {
  if (!oledOk || !P.faceOn) return;
  if (T.state == ST_ATTACK || T.state == ST_EDGE) return;   // barramento e dos olhos

  oled.clearDisplay();
  oled.setTextSize(1); oled.setCursor(0, 0);
  oled.print(P.mode == MODE_LESMA ? F("LESMA") : P.mode == MODE_CAPIROTO ? F("CAPIROTO") : F("NORMAL"));
  oled.setCursor(70, 0);
  oled.print(T.armed ? F("ARMADO") : F("parado"));
  oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // olhos: "> <" com raiva, "o_o" acordado, "- -" dormindo
  oled.setTextSize(3); oled.setCursor(20, 24);
  if (!T.armed)                    oled.print(F("- -"));
  else if (T.state == ST_LOCK)     oled.print(F("O O"));
  else if (T.state == ST_COUNTDOWN)oled.print(F("o o"));
  else                             oled.print(F("o_o"));

  oled.setTextSize(1); oled.setCursor(0, 56);
  oled.print(T.distL); oled.print(F("cm  ")); oled.print(T.distR); oled.print(F("cm"));
  oled.display();
}

} // namespace Face
#endif

// ---------------------------------------------------------------------
//  CEREBRO
// ---------------------------------------------------------------------
namespace Brain {

uint8_t  st = ST_IDLE;
uint32_t stSince = 0, armedAt = 0;
int8_t   sweepDir = 1;
uint8_t  hits = 0, misses = 0;
int16_t  lastFused = -1;
float    iAcc = 0, ePrev = 0;
uint32_t edgeUntil = 0;
uint8_t  edgeFase = 0;
int8_t   edgeLado = 0;

void go(uint8_t s) { st = s; stSince = millis(); T.state = s; }

void applyMode() {
  T.mode = P.mode;
  switch (P.mode) {
    case MODE_LESMA:
      P.vSearch = 48; P.vAttack = 104; P.vMax = 94; P.confirmHits = 5; break;
    case MODE_CAPIROTO:
      P.vSearch = 128; P.vAttack = 255; P.vMax = 255; P.confirmHits = 2; break;
    default:
      P.vSearch = 107; P.vAttack = 230; P.vMax = 209; P.confirmHits = 3; break;
  }
}

void disarm() {
  T.armed = false;
  Mot::aplica(0, 0);
  Mot::wake(false);
  go(ST_IDLE);
  Serial.println(F("[brain] desarmado"));
}

void arm() {
  applyMode();
  T.armed = true;
  armedAt = millis();
  Mot::wake(true);
  iAcc = 0; ePrev = 0; hits = 0; misses = 0;
  go(ST_COUNTDOWN);
  Serial.println(F("[brain] ARMADO - contagem"));
}

// Manobra de borda. Nao bloqueia: cada fase tem um prazo e o loop volta.
void iniciaBorda(bool esq, bool dir) {
  edgeLado = esq && dir ? 0 : (esq ? -1 : 1);
  edgeFase = 0;
  edgeUntil = millis() + 18;          // freio eletrico curto
  Mot::freio();
  T.nEdgeSaves++;
  Snd::beep(1800, 60);
  go(ST_EDGE);
}

void passoBorda() {
  if ((int32_t)(millis() - edgeUntil) < 0) return;
  switch (edgeFase) {
    case 0:                                       // recua reto
      Mot::aplica(-(int16_t)P.vReverse, -(int16_t)P.vReverse);
      edgeUntil = millis() + P.edgeBackMs;
      edgeFase = 1;
      break;
    case 1:                                       // gira para dentro
      if (edgeLado < 0)      Mot::aplica(P.vReverse, -(int16_t)P.vReverse);
      else if (edgeLado > 0) Mot::aplica(-(int16_t)P.vReverse, P.vReverse);
      else                   Mot::aplica(P.vReverse, -(int16_t)P.vReverse);  // meia-volta
      edgeUntil = millis() + (edgeLado == 0 ? P.edgeTurnMs * 2 : P.edgeTurnMs);
      edgeFase = 2;
      break;
    default:
      hits = 0; misses = 0;
      go(ST_SEARCH);
      break;
  }
}

// PID sobre a diferenca entre os olhos. Erro positivo = alvo a direita.
int16_t pid(int16_t dl, int16_t dr) {
  float e;
  if (dl >= 0 && dr >= 0)      e = (float)(dl - dr);
  else if (dl >= 0)            e = -40.0f;      // so o esquerdo ve: vire para la
  else if (dr >= 0)            e =  40.0f;
  else                         e = 0.0f;

  iAcc = constrain(iAcc + e * 0.005f, -60.0f, 60.0f);
  float d = e - ePrev; ePrev = e;
  float out = P.kp * e + P.ki * iAcc + P.kd * d;
  return clampi((int32_t)out, -PWM_MAX, PWM_MAX);
}

void tick() {
  uint32_t agora = millis();

  // ---- guarda de borda: vem antes de tudo -------------------------
  if (T.armed && (T.irL || T.irR) && st != ST_EDGE) {
    iniciaBorda(T.irL, T.irR);
    return;
  }

  switch (st) {
    case ST_COUNTDOWN: {
      uint32_t falta = P.countdownMs - min(P.countdownMs, (uint16_t)(agora - armedAt));
      static uint8_t ultimoBip = 255;
      uint8_t seg = falta / 1000;
      if (seg != ultimoBip) { ultimoBip = seg; Snd::beep(seg ? 1200 : 2200, seg ? 70 : 250); }
      Mot::aplica(0, 0);
      if (agora - armedAt >= P.countdownMs) { go(ST_SEARCH); }
      break;
    }

    case ST_SEARCH: {
      if (agora - stSince > P.sweepMs) { sweepDir = -sweepDir; stSince = agora; }
      Mot::aplica(sweepDir * (int16_t)P.vSearch, -sweepDir * (int16_t)P.vSearch);
      int16_t f = (T.distL >= 0 && T.distR >= 0) ? min(T.distL, T.distR)
                : (T.distL >= 0 ? T.distL : T.distR);
      if (f >= 0) { hits = 1; lastFused = f; go(ST_LOCK); }
      break;
    }

    case ST_LOCK: {
      int16_t f = (T.distL >= 0 && T.distR >= 0) ? min(T.distL, T.distR)
                : (T.distL >= 0 ? T.distL : T.distR);
      if (f < 0) {
        if (++misses >= P.loseMisses) { misses = 0; T.nLost++; go(ST_SEARCH); }
      } else {
        // salto grande entre leituras e ruido, nao alvo
        if (lastFused >= 0 && abs(f - lastFused) > (int16_t)P.jumpCm) { hits = 0; }
        else if (++hits >= P.confirmHits) {
          T.nAttacks++; Snd::beep(2000, 80); go(ST_ATTACK);
        }
        lastFused = f; misses = 0;
      }
      // avanca devagar enquanto confirma
      int16_t corr = pid(T.distL, T.distR);
      Mot::aplica(P.vSearch + corr / 2, P.vSearch - corr / 2);
      break;
    }

    case ST_ATTACK: {
      int16_t f = (T.distL >= 0 && T.distR >= 0) ? min(T.distL, T.distR)
                : (T.distL >= 0 ? T.distL : T.distR);
      if (f < 0) {
        if (++misses >= P.loseMisses) { misses = 0; T.nLost++; go(ST_SEARCH); }
      } else misses = 0;

      int16_t corr = pid(T.distL, T.distR);
      // colado no alvo: corta a correcao e empurra reto
      if (f >= 0 && f <= 18) corr = corr * 3 / 10;
      Mot::aplica(P.vAttack + corr, P.vAttack - corr);
      break;
    }

    case ST_EDGE:  passoBorda(); break;
    case ST_FAULT: Mot::aplica(0, 0); break;
    default:       Mot::aplica(0, 0); break;
  }
}

} // namespace Brain

// ---------------------------------------------------------------------
//  CONSOLE SERIAL - a unica interface que sobrou
// ---------------------------------------------------------------------
uint32_t monAte = 0;

void ajuda() {
  Serial.println(F("[cmd] a=armar/parar  s=autoteste  e=exame dos olhos"));
  Serial.println(F("[cmd] b=buzzer  m=monitor 20Hz por 10s  p=ajustar trimpot do IR"));
  Serial.println(F("[cmd] 1/2/3=modo NORMAL/LESMA/CAPIROTO  ?=ajuda"));
}

void console() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'a': if (T.armed) Brain::disarm(); else Brain::arm(); break;
      case 's': autoteste(); break;
      case 'e': exameOlhos(); break;
      case 'b': Snd::beep(1500, 150); break;
      case 'm': monAte = millis() + 10000; Serial.println(F("#MONINI")); break;
      case 'p':                                  // ajuste do trimpot
        monAte = millis() + 60000;
        Serial.println(F("[pot] 60 s de leitura. Gire o trimpot ate o OUT virar"));
        Serial.println(F("[pot] sobre a zona ESCURA, e confira que volta sobre a CLARA"));
        break;
      case '1': P.mode = MODE_NORMAL;   Brain::applyMode(); Serial.println(F("[modo] NORMAL")); break;
      case '2': P.mode = MODE_LESMA;    Brain::applyMode(); Serial.println(F("[modo] LESMA")); break;
      case '3': P.mode = MODE_CAPIROTO; Brain::applyMode(); Serial.println(F("[modo] CAPIROTO")); break;
      case '?': ajuda(); break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println(F("\n===== ROBO SUMO - Arduino Nano (porte bruto) ====="));

  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  Mot::begin();
  Serial.println(F("[boot] motores prontos (dormindo)"));

  Wire.begin();
  Wire.setClock(400000L);

#if USAR_OLED
  Face::begin();
#endif

  Sens::begin();
  autoteste();
  Brain::applyMode();
  Brain::go(ST_IDLE);

  Serial.println(F("===== pronto ====="));
  ajuda();
  Snd::beep(1000, 120);
}

void loop() {
  static uint32_t hzT = 0, dbgT = 0, faceT = 0, monT = 0;
  static uint16_t hzN = 0;

  // A borda vem primeiro, sempre. Depois dela nada mais importa neste
  // ciclo: sem RTOS, a unica garantia de reacao rapida e esta ordem.
  Sens::irPoll();
  if (bordaFlag) { bordaFlag = false; }     // a bandeira so acorda; o teste e o de cima

  Brain::tick();
  Sens::tofPoll();
  Snd::update();

  // nFAULT das pontes: baixo = protecao atuou
  T.drvFault = (digitalRead(PIN_DRV_FAULT) == LOW);
  if (T.drvFault && T.armed) { Brain::disarm(); Brain::go(ST_FAULT); Serial.println(F("[drv] nFAULT!")); }

  // botao: toque curto arma/desarma
  static bool btnAnt = HIGH;
  bool btn = digitalRead(PIN_BTN);
  if (btnAnt == HIGH && btn == LOW) { if (T.armed) Brain::disarm(); else Brain::arm(); }
  btnAnt = btn;

  digitalWrite(PIN_LED, T.armed ? ((millis() / 200) % 2) : LOW);

  console();

  hzN++;
  if (millis() - hzT >= 1000) { T.loopHz = hzN; hzN = 0; hzT = millis(); }

  // monitor de 20 Hz, para julgar ESTABILIDADE - o que estraga a
  // investida nao e distancia errada, e distancia que pula sozinha
  if (monAte) {
    if ((int32_t)(millis() - monAte) >= 0) { monAte = 0; Serial.println(F("#MONFIM")); }
    else if (millis() - monT >= 50) {
      monT = millis();
      Serial.print(F("#M ")); Serial.print(millis());
      Serial.print(' '); Serial.print(T.distL);
      Serial.print(' '); Serial.print(T.distR);
      Serial.print(' '); Serial.print(T.irLraw);
      Serial.print(' '); Serial.print(T.irRraw);
      Serial.print(' '); Serial.print(T.irLdo);
      Serial.print(' '); Serial.println(T.irRdo);
    }
  }

  if (millis() - dbgT >= 1000) {
    dbgT = millis();
    Serial.print(F("#D st=")); Serial.print(T.state);
    Serial.print(F(" armed="));Serial.print(T.armed);
    Serial.print(F(" hz="));   Serial.print(T.loopHz);
    Serial.print(F(" dL="));   Serial.print(T.distL);
    Serial.print(F(" dR="));   Serial.print(T.distR);
    Serial.print(F(" tofL=")); Serial.print(T.tofOkL);
    Serial.print(F(" tofR=")); Serial.print(T.tofOkR);
    Serial.print(F(" irLa=")); Serial.print(T.irLraw);
    Serial.print(F(" irRa=")); Serial.print(T.irRraw);
    Serial.print(F(" irL="));  Serial.print(T.irL);
    Serial.print(F(" irR="));  Serial.print(T.irR);
    Serial.print(F(" vbat=")); Serial.print(T.vbat);
    Serial.print(F(" flt="));  Serial.println(T.drvFault);
    Sens::vbatPoll();
  }

#if USAR_OLED
  if (millis() - faceT >= 120) { faceT = millis(); Face::render(); }
#endif
}
