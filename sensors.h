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
//  DIAGNOSTICO DE BANCADA
//  O autoteste nao fica so na serial: o resultado mora aqui para o painel
//  web servir em /api/selftest. Assim da para conferir fiacao pelo celular,
//  com o robo ja fechado e longe do cabo USB.
// ---------------------------------------------------------------------
struct Diag {
  uint8_t  nOled, nTof;          // quantos dispositivos cada barramento tem
  uint8_t  aOled[8], aTof[8];    // enderecos encontrados (ate 8 por barramento)
  bool     irLdo, irRdo;         // pinos DO no momento da varredura
  uint16_t irLa, irRa;           // pinos AO (ADC cru)
  uint32_t at;                   // millis da ultima varredura
};
Diag DG = {};

// Enquanto isto e true, quem usa I2C sai da frente: uma varredura no meio
// de um quadro do OLED ou de uma leitura continua do ToF suja as duas coisas.
volatile bool diagBusy = false;

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
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)0xC0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return false;
  return (Wire.read() == 0xEE);
}

static bool configura(VL53L0X& s) {
  s.setMeasurementTimingBudget(TOF_BUDGET_US);
  s.startContinuous(0);               // costas-com-costas, sem pausa
  return true;
}

// O init() do VL53L0X nao e uma escrita: sao dezenas de acessos I2C em
// sequencia, e basta um falhar para ele devolver false com o sensor
// perfeitamente vivo. Na bancada isso apareceu de forma intermitente
// assim que o OLED entrou no mesmo barramento - um boot subia os dois
// olhos, o seguinte dizia "respondeu mas o init falhou" no direito.
//
// Por isso a subida tem tres tentativas, cada uma com um reset de
// verdade pelo XSHUT no meio. Desistir na primeira custaria um olho e a
// triangulacao inteira por causa de um unico acesso perdido.
#define TOF_TENTATIVAS 3

static bool bringUp(VL53L0X& s, uint8_t xshut, uint8_t addr, const char* nome) {
  for (uint8_t tent = 1; tent <= TOF_TENTATIVAS; tent++) {
    digitalWrite(xshut, LOW);         // reset limpo antes de cada tentativa
    delay(5);
    digitalWrite(xshut, HIGH);        // solta o reset deste sensor
    delay(10);                        // t_BOOT e 1,2 ms max (§2.9.1)

    if (!tofResponde(TOF_ADDR_R)) {
      Serial.printf("[tof] %s NAO respondeu (0xC0 != 0xEE) [tentativa %u]\n", nome, tent);
      continue;
    }
    s.setBus(&Wire);
    s.setTimeout(150);
    if (!s.init()) {
      Serial.printf("[tof] %s respondeu mas o init falhou [tentativa %u]\n", nome, tent);
      continue;
    }
    goto subiu;
  }
  Serial.printf("[tof] %s desistiu depois de %u tentativas\n", nome, (unsigned)TOF_TENTATIVAS);
  digitalWrite(xshut, LOW);           // desliga p/ nao atrapalhar o outro
  return false;

subiu:;
  if (addr != TOF_ADDR_R) s.setAddress(addr);
  configura(s);
  Serial.printf("[tof] %s ok no endereco 0x%02X\n", nome, addr);
  return true;
}

bool wire1Pronto = false;

// Quantos VL53L0X respondem AGORA, checando identidade e nao so ACK.
// Serve para perguntar "ha olho acordado?" num barramento onde tambem
// vivem OLED e o que mais aparecer.
static uint8_t olhosVivos(bool falar) {
  uint8_t v = 0;
  if (tofResponde(TOF_ADDR_R)) { v++; if (falar) Serial.println("[tof] 0x29 responde com os dois XSHUT em reset"); }
  if (tofResponde(TOF_ADDR_L)) { v++; if (falar) Serial.println("[tof] 0x30 responde com os dois XSHUT em reset"); }
  return v;
}

void tofBegin() {
  pinMode(PIN_TOF_L_XSHUT, OUTPUT);
  pinMode(PIN_TOF_R_XSHUT, OUTPUT);
  digitalWrite(PIN_TOF_L_XSHUT, LOW);     // os dois em reset
  digitalWrite(PIN_TOF_R_XSHUT, LOW);
  delay(20);

  if (!wire1Pronto) { Wire.begin(PIN_TOF_SDA, PIN_TOF_SCL, 400000); wire1Pronto = true; }

  // Teste de XSHUT: com os dois em reset, nenhum OLHO pode responder.
  //
  // Isto ja foi escrito como "o barramento tem que estar VAZIO", e essa
  // formulacao morreu no dia em que os olhos passaram para 21/22: o OLED
  // mora aqui e responde sempre. O firmware via o 0x3C, concluia "XSHUT
  // nao esta ligado" e caia em modo caolho com dois olhos bons na mesa.
  //
  // A pergunta certa nunca foi "ha alguem no barramento" - e "ha um
  // VL53L0X". Quem separa os dois casos e o registrador 0xC0, que vale
  // 0xEE em todo VL53L0X (Tab. 4) e nao existe no SSD1306.
  uint8_t vivos = olhosVivos(true);

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
    tofL.setBus(&Wire);
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
    // Varredura de diagnostico em andamento: sai do barramento e espera.
    if (diagBusy) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

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
      T.tofRawL = (int16_t)(mm > 32000 ? 32000 : mm);   // sempre o cru
      if (ok) T.tofFailL = 0; else if (T.tofFailL < 60000) T.tofFailL++;
      cmL = cm; T.distL = cm;
    }
    vTaskDelay(pdMS_TO_TICKS(4));

    if (haveR) {
      uint16_t mm = tofR.readRangeContinuousMillimeters();
      bool ok; int16_t cm = toCm(tofR, mm, ok);
      T.tofRawR = (int16_t)(mm > 32000 ? 32000 : mm);   // sempre o cru
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
  // NAO se le mais o ADC aqui, por duas razoes independentes e ambas
  // definitivas:
  //
  //  1. Os modulos IR deste robo sao de TRES fios (VCC/GND/OUT). Nao
  //     existe saida analogica para ler - o limiar mora no trimpot do
  //     modulo, nao no software.
  //  2. GPIO 32 e 33, que eram o "AO esquerdo/direito", viraram as
  //     sondas de tensao do modulo ToF. Ler o ADC deles agora nao seria
  //     so inutil: seria decidir borda a partir da alimentacao de outro
  //     sensor.
  //
  // Isso ja custou caro: com irSource=1 (padrao antigo) o robo declarava
  // BORDA a partir de pino flutuante e ninguem entendia por que.
  uint16_t a = 0, b = 0;
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
//  CALIBRACAO DO IR
//
//  Antes disto era preciso olhar dois numeros no painel, tirar a media de
//  cabeca e digitar no slider. Agora o robo mede: poe na zona escura e
//  captura; poe na faixa clara e captura. O limiar vira o meio do caminho
//  e o SENTIDO (a borda e o valor baixo ou o alto?) sai da propria medida,
//  em vez de chute - e essa e a parte que mais se erra, porque modulo IR
//  invertido faz o robo fugir do centro da arena e correr para a borda.
// ---------------------------------------------------------------------
struct IrCal {
  uint16_t escL, escR;      // zona segura (escura)
  uint16_t claL, claR;      // borda (clara)
  bool     temEsc, temCla;
  int16_t  margem;          // separacao entre as duas leituras
};
IrCal CAL = {};

// A media sozinha nao basta: 15 ms de amostragem nao distinguem "sensor
// parado sobre a superficie" de "robo sendo movido durante a captura", e
// uma calibracao feita em movimento e pior que nenhuma - ela parece boa.
// Entao a captura mede por ~600 ms e devolve TAMBEM a excursao, para quem
// chamou poder recusar o valor.
static uint16_t mediaAdc(uint8_t pino, uint16_t* excursao = nullptr) {
  uint32_t acc = 0;
  uint16_t mn = 4095, mx = 0;
  const uint8_t N = 60;
  for (uint8_t i = 0; i < N; i++) {
    uint16_t v = analogRead(pino);
    acc += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    delay(10);                      // 60 x 10 ms = ~600 ms de janela
  }
  if (excursao) *excursao = mx - mn;
  return (uint16_t)(acc / N);
}

// Mede os dois lados e reclama se a leitura nao estava firme.
static bool capturaPar(uint16_t& esq, uint16_t& dir, const char* qual) {
  uint16_t exE = 0, exD = 0;
  esq = mediaAdc(PIN_IR_L_A, &exE);
  dir = mediaAdc(PIN_IR_R_A, &exD);
  Serial.printf("[ircal] %s: esq=%u (variou %u) dir=%u (variou %u)\n",
                qual, esq, exE, dir, exD);
  if (exE > 150 || exD > 150) {
    Serial.printf("[ircal] LEITURA INSTAVEL durante a captura (esq %u, dir %u de excursao)."
                  " Segure o robo parado sobre a superficie e capture de novo -"
                  " calibrar em movimento e pior que nao calibrar.\n", exE, exD);
    return false;
  }
  return true;
}

static void fecharCal() {
  if (!(CAL.temEsc && CAL.temCla)) return;

  int32_t esc = ((int32_t)CAL.escL + CAL.escR) / 2;
  int32_t cla = ((int32_t)CAL.claL + CAL.claR) / 2;

  P.irActiveLow = (cla < esc) ? 1 : 0;              // a borda e o lado mais baixo?
  P.irThreshold = (uint16_t)((esc + cla) / 2);
  CAL.margem    = (int16_t)(esc > cla ? esc - cla : cla - esc);

  Serial.printf("[ircal] escuro %u/%u | claro %u/%u -> limiar %u, borda = ADC %s\n",
                CAL.escL, CAL.escR, CAL.claL, CAL.claR, P.irThreshold,
                P.irActiveLow ? "BAIXO" : "ALTO");

  // As duas checagens que separam calibracao boa de calibracao que so
  // parece boa na bancada e falha na arena:
  if (CAL.margem < 250)
    Serial.printf("[ircal] SEPARACAO PEQUENA (%d). Aproxime o sensor do chao "
                  "(3 a 5 mm) ou confirme que a faixa e mesmo clara.\n", CAL.margem);

  int32_t difEsc = (int32_t)CAL.escL - CAL.escR; if (difEsc < 0) difEsc = -difEsc;
  int32_t difCla = (int32_t)CAL.claL - CAL.claR; if (difCla < 0) difCla = -difCla;
  if (difEsc > 400 || difCla > 400)
    Serial.printf("[ircal] OS DOIS LADOS NAO CONCORDAM (esc %d, claro %d). "
                  "Alturas diferentes em relacao ao chao - um limiar so vai "
                  "servir mal para os dois.\n", (int)difEsc, (int)difCla);
}

void calEscuro() {
  CAL.temEsc = capturaPar(CAL.escL, CAL.escR, "zona ESCURA");
  if (CAL.temEsc) fecharCal();
}

void calClaro() {
  CAL.temCla = capturaPar(CAL.claL, CAL.claR, "faixa CLARA");
  if (CAL.temCla) fecharCal();
}

// Zerar tem que desfazer TAMBEM o que a calibracao derivou. Limpar so a
// captura deixaria para tras um limiar e um sentido de sinal calculados a
// partir de medidas que o usuario acabou de descartar.
void calLimpa() {
  CAL = IrCal{};
  P.irThreshold = P_DEFAULT.irThreshold;
  P.irActiveLow = P_DEFAULT.irActiveLow;
  Serial.printf("[ircal] calibracao zerada - limiar volta a %u, borda = ADC %s\n",
                P.irThreshold, P.irActiveLow ? "BAIXO" : "ALTO");
}

// ---------------------------------------------------------------------
//  BATERIA
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
//  AUTOTESTE (roda uma vez no boot)
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
//  SONDA ELETRICA DAS LINHAS I2C
//
//  A varredura I2C responde "ninguem respondeu", e isso tem pelo menos
//  tres causas que exigem acoes opostas: nada ligado, ligado sem
//  alimentacao, e linha em curto. Dá para separar as tres ANTES de
//  qualquer transacao, porque I2C depende de pull-up EXTERNO - e quem
//  traz esse pull-up e o modulo.
//
//    linha solta (sem pull interno) fica em ALTO  -> ha pull-up externo,
//                                                    ou seja, modulo
//                                                    presente E alimentado
//    solta em BAIXO, mas ALTO com pull interno    -> linha aberta: nada
//                                                    ligado, ou fio solto
//    BAIXO ate com pull interno                   -> curto para o terra,
//                                                    ou escravo travado
//                                                    segurando a linha
// ---------------------------------------------------------------------
struct SondaLinha { uint8_t soltoAlto, pullAlto; };   // de 32 amostras

// A medida so vale se a linha for DESCARREGADA antes. Soltar o pino e ler
// em seguida nao prova nada: a capacitancia da propria linha segura o
// nivel alto por milissegundos, e uma linha flutuante daria exatamente o
// mesmo "alto" de uma linha com pull-up. Entao: puxa para baixo, solta, e
// ve quem levanta. Pull-up externo de 10k levanta em microssegundos;
// linha solta fica em baixo, porque so tem fuga para levanta-la.
static SondaLinha amostraPino(uint8_t pino) {
  SondaLinha r = {0, 0};

  pinMode(pino, OUTPUT); digitalWrite(pino, LOW);   // descarrega
  delayMicroseconds(1500);
  pinMode(pino, INPUT);                              // solta e observa
  delayMicroseconds(80);
  for (uint8_t i = 0; i < 32; i++) { if (digitalRead(pino)) r.soltoAlto++; delayMicroseconds(120); }

  pinMode(pino, OUTPUT); digitalWrite(pino, LOW);   // descarrega de novo
  delayMicroseconds(1500);
  pinMode(pino, INPUT_PULLUP);
  delayMicroseconds(80);
  for (uint8_t i = 0; i < 32; i++) { if (digitalRead(pino)) r.pullAlto++; delayMicroseconds(120); }

  pinMode(pino, INPUT);
  return r;
}

static const char* veredito(const SondaLinha& a) {
  // Atencao: isto NAO separa um pull-up de 10k de um fio ligado direto no
  // 3V3. Nos dois casos alguem segura a linha em alto. So a varredura I2C
  // desempata.
  if (a.soltoAlto >= 29) return "alguem segura a linha em ALTO (pull-up do modulo, ou fio no 3V3)";
  if (a.pullAlto  >= 29) return "linha ABERTA - nada ligado ou fio solto";
  return "presa em BAIXO - curto para o terra ou escravo travado";
}

// Solta o barramento, mede, devolve. So faz sentido com diagBusy ligado.
void sondaLinhas(TwoWire& bus, uint8_t sda, uint8_t scl, const char* nome) {
  bus.end();
  SondaLinha a = amostraPino(sda);
  SondaLinha b = amostraPino(scl);
  Serial.printf("[teste] linhas de %s:\n", nome);
  Serial.printf("[teste]   SDA solto=%u/32 alto, com pull=%u/32 -> %s\n",
                a.soltoAlto, a.pullAlto, veredito(a));
  Serial.printf("[teste]   SCL solto=%u/32 alto, com pull=%u/32 -> %s\n",
                b.soltoAlto, b.pullAlto, veredito(b));
  bus.begin(sda, scl, 400000);
}

static uint8_t scanBus(TwoWire& bus, const char* nome, uint8_t* out) {
  Serial.printf("[teste] varrendo %s:", nome);
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      if (n < 8) out[n] = a;
      n++;
    }
  }
  if (!n) Serial.print("  NENHUM DISPOSITIVO");
  Serial.println();
  return n;
}

// Escravo interrompido no meio de um byte fica segurando SDA em baixo, e
// nesse estado a varredura mostra o barramento VAZIO - nao "com defeito",
// vazio, como se nada estivesse ligado. Reiniciar a ESP32 nao resolve:
// quem esta travado e o outro lado. A cura e a da norma I2C: pulsar SCL
// ate o escravo terminar o byte, e fechar com um STOP.
static bool destrava(uint8_t sda, uint8_t scl, const char* nome) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  if (digitalRead(sda)) return true;

  Serial.printf("[i2c] %s: SDA presa em BAIXO, pulsando SCL\n", nome);
  for (uint8_t i = 0; i < 18 && !digitalRead(sda); i++) {
    pinMode(scl, OUTPUT); digitalWrite(scl, LOW); delayMicroseconds(10);
    pinMode(scl, INPUT_PULLUP);                   delayMicroseconds(10);
  }
  pinMode(sda, OUTPUT); digitalWrite(sda, LOW); delayMicroseconds(10);   // STOP
  pinMode(scl, INPUT_PULLUP);                   delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP);                   delayMicroseconds(10);

  bool livre = digitalRead(sda);
  Serial.printf("[i2c] %s: %s\n", nome,
                livre ? "liberado" : "SDA CONTINUA baixa - e curto para GND, nao travamento");
  return livre;
}

void destravaBarramentos() {
  diagBusy = true;
  vTaskDelay(pdMS_TO_TICKS(40));
  Wire.end();
  Wire.end();
  destrava(PIN_SDA,     PIN_SCL,     "OLED (21/22)");
  destrava(PIN_TOF_SDA, PIN_TOF_SCL, "olhos (16/17)");
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  Wire.begin(PIN_TOF_SDA, PIN_TOF_SCL, 400000);
  uint8_t tmp[8];
  scanBus(Wire,  "OLED depois de destravar",  tmp);
  scanBus(Wire, "olhos depois de destravar", tmp);
  diagBusy = false;
  tofBegin();
}

void selfTest() {
  diagBusy = true;
  vTaskDelay(pdMS_TO_TICKS(40));      // deixa terminar o quadro do OLED em curso
  Serial.println("[teste] --- autoteste ---");

  // Identidade do modulo. Importa por um motivo pratico: em placas com
  // PSRAM (ESP32-WROVER) os GPIO 16 e 17 sao do chip de PSRAM e nao
  // servem de I2C - dariam exatamente o sintoma "linha com pull-up e
  // ninguem responde".
  Serial.printf("[teste] chip=%s rev=%u nucleos=%u flash=%uMB psram=%uKB\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
                (unsigned)(ESP.getPsramSize() / 1024));

  // Os pinos dos olhos obedecem mesmo? Se um pino recusa ir a BAIXO, tem
  // outro dono - PSRAM, outro periferico, ou um curto para o 3V3.
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t pino = i ? PIN_TOF_SCL : PIN_TOF_SDA;
    // Janela curta de proposito: se o pino estiver preso no 3V3 por um fio,
    // manda-lo a BAIXO e um curto atraves do transistor de saida da ESP32.
    // 60 us bastam para a leitura e mantem a corrente em um pulso curto.
    pinMode(pino, OUTPUT);
    digitalWrite(pino, LOW);  delayMicroseconds(60); bool leBaixo = digitalRead(pino);
    digitalWrite(pino, HIGH); delayMicroseconds(60); bool leAlto  = digitalRead(pino);
    pinMode(pino, INPUT);
    Serial.printf("[teste] GPIO %u (olhos %s): mandei BAIXO li %s, mandei ALTO li %s -> %s\n",
                  pino, i ? "SCL" : "SDA",
                  leBaixo ? "ALTO" : "baixo", leAlto ? "alto" : "BAIXO",
                  (!leBaixo && leAlto) ? "pino obedece" : "PINO NAO OBEDECE");
  }
  // Barramento unico: OLED e olhos dividem 21/22, entao uma varredura so
  // responde pelos dois campos do painel.
  DG.nOled = scanBus(Wire, "I2C unico - OLED e olhos (SDA 21 / SCL 22)", DG.aOled);
  DG.nTof  = DG.nOled;
  memcpy(DG.aTof, DG.aOled, sizeof(DG.aTof));

  // A pergunta seguinte e eletrica, nao logica. Com um barramento so, o
  // controle POSITIVO de antes (o outro barramento) deixou de existir -
  // resta o controle NEGATIVO, o GPIO 5, que este projeto nao usa. Ele
  // continua sendo indispensavel: se um pino livre acusar "pull-up
  // externo", a sonda esta mentindo e nenhuma conclusao dela vale.
  sondaLinhas(Wire, PIN_SDA, PIN_SCL, "o barramento unico (21/22)");
  {
    SondaLinha c = amostraPino(5);
    Serial.printf("[teste] GPIO 5 livre [controle -]: solto=%u/32 alto, com pull=%u/32 -> %s\n",
                  c.soltoAlto, c.pullAlto, veredito(c));
  }
  DG.irLdo = digitalRead(PIN_IR_L_D);
  DG.irRdo = digitalRead(PIN_IR_R_D);
  DG.irLa  = 0;      // modulo de 3 fios: nao ha AO
  DG.irRa  = 0;
  DG.at    = millis();
  Serial.printf("[teste] IR DO esq (GPIO34): %s | IR DO dir (GPIO35): %s\n",
                DG.irLdo ? "ALTO" : "BAIXO", DG.irRdo ? "ALTO" : "BAIXO");
  // Os pinos de IR sao ADC1 (34 = CH6, 35 = CH7), entao da para medir a
  // TENSAO que chega neles, e nao so o nivel logico. Isso separa duas
  // coisas que o digitalRead confunde num pino sem pull-up interno:
  //   ~0 V   -> nao chega sinal nenhum (fio partido, ou modulo mudo)
  //   ~3,3 V lido como BAIXO -> o pino recebe o sinal e nao o enxerga:
  //           a entrada digital daquele GPIO e que esta comprometida.
  {
    uint16_t mvL = analogReadMilliVolts(PIN_IR_L_D);
    uint16_t mvR = analogReadMilliVolts(PIN_IR_R_D);
    // DEVOLVER o pino para a funcao digital. analogRead() na ESP32 roteia
    // o pino para o ADC e o digitalRead seguinte passa a mentir - medir
    // uma vez cegava os dois sensores de borda ate o proximo boot.
    pinMode(PIN_IR_L_D, INPUT_PULLUP);
    pinMode(PIN_IR_R_D, INPUT);
    delayMicroseconds(50);
    bool dL = digitalRead(PIN_IR_L_D), dR = digitalRead(PIN_IR_R_D);
    Serial.printf("[teste] IR esq GPIO%u = %u mV, digital %s | dir GPIO%u = %u mV, digital %s\n",
                  PIN_IR_L_D, mvL, dL ? "ALTO" : "BAIXO",
                  PIN_IR_R_D, mvR, dR ? "ALTO" : "BAIXO");
    Serial.println("[teste]   cuidado: com 11 dB o ADC satura perto de 3100 mV; leitura"
                   " identica nos dois pinos e teto de escala, nao prova de sinal");
  }
  Serial.printf("[teste] receptor do juiz (GPIO%u) = %s (repouso e ALTO)\n",
                PIN_IR_JUIZ, digitalRead(PIN_IR_JUIZ) ? "ALTO" : "BAIXO");
  Serial.println("[teste] --- fim ---");
  diagBusy = false;
}

// ---------------------------------------------------------------------
//  EXAME DOS OLHOS
//
//  "Pull-up presente + ninguem responde" tem varias causas, e elas pedem
//  acoes opostas. Este exame varre o barramento em cada condicao que as
//  separa, em vez de deixar a gente adivinhando:
//
//    XSHUT em BAIXO   -> tem que dar VAZIO. Se responder aqui, o XSHUT
//                        nao esta chegando no modulo.
//    XSHUT em ALTO    -> tem que aparecer 0x29. E o caso normal.
//    XSHUT solto      -> o modulo tem pull-up proprio no XSHUT. Se so
//                        assim responder, o fio de XSHUT esta partido:
//                        o nivel ALTO da ESP32 nao chega la.
//    100 kHz          -> jumper longo demais nao fecha em 400 kHz.
//    SDA/SCL trocados -> troca classica de fio. O pull-up aparece nas
//                        duas linhas de qualquer jeito, entao a sonda
//                        eletrica NAO pega esse erro - so a varredura.
// ---------------------------------------------------------------------
void exameOlhos() {
  diagBusy = true;
  vTaskDelay(pdMS_TO_TICKS(40));
  uint8_t tmp[8];
  Serial.println("[exame] --- exame dos olhos ---");

  pinMode(PIN_TOF_L_XSHUT, OUTPUT);
  pinMode(PIN_TOF_R_XSHUT, OUTPUT);

  digitalWrite(PIN_TOF_L_XSHUT, LOW);
  digitalWrite(PIN_TOF_R_XSHUT, LOW);
  delay(25);
  scanBus(Wire, "XSHUT em BAIXO (esperado: so o OLED)", tmp);
  // Conta olhos, nao dispositivos: o OLED responde aqui e nao e defeito.
  uint8_t nBaixo = olhosVivos(false);

  digitalWrite(PIN_TOF_L_XSHUT, HIGH);
  digitalWrite(PIN_TOF_R_XSHUT, HIGH);
  delay(25);
  uint8_t nAlto = scanBus(Wire, "XSHUT em ALTO (esperado: 0x29)", tmp);

  pinMode(PIN_TOF_L_XSHUT, INPUT);
  pinMode(PIN_TOF_R_XSHUT, INPUT);
  delay(25);
  uint8_t nSolto = scanBus(Wire, "XSHUT solto (pull-up do modulo)", tmp);

  Wire.end();
  Wire.begin(PIN_TOF_SDA, PIN_TOF_SCL, 100000);
  delay(10);
  uint8_t nLento = scanBus(Wire, "mesmos pinos, porem a 100 kHz", tmp);

  Wire.end();
  Wire.begin(PIN_TOF_SCL, PIN_TOF_SDA, 100000);      // de proposito ao contrario
  delay(10);
  uint8_t nTrocado = scanBus(Wire, "SDA/SCL TROCADOS (21 <-> 22)", tmp);

  Wire.end();
  Wire.begin(PIN_TOF_SDA, PIN_TOF_SCL, 400000);

  // Matriz de curtos entre os quatro pinos de I2C. Um curto entre SDA e
  // SCL deixa as duas linhas com pull-up (o do modulo alimenta as duas) e
  // torna a comunicacao impossivel - exatamente o quadro "tem pull-up e
  // ninguem responde". Aqui isso vira medida: puxo um pino para baixo e
  // vejo se algum outro desce junto. So descem juntos se estiverem ligados.
  {
    // Com barramento unico sobraram DOIS pinos, nao quatro. Comparar um
    // pino com ele mesmo acusaria "curto" sempre - foi o que aconteceu
    // aqui na primeira execucao depois da mudanca, e o relatorio ficou
    // dizendo que 21 estava em curto com 21 sob nomes diferentes.
    const uint8_t pinos[2] = { PIN_SDA, PIN_SCL };
    const char*   nomes[2] = { "21 SDA", "22 SCL" };
    Wire.end();
    bool achou = false;
    for (uint8_t i = 0; i < 2; i++) {
      uint8_t j = 1 - i;
      pinMode(pinos[j], INPUT_PULLUP);
      pinMode(pinos[i], OUTPUT); digitalWrite(pinos[i], LOW);
      delayMicroseconds(800);
      if (digitalRead(pinos[j]) == LOW) {
        Serial.printf("[exame]   CURTO: %s esta ligado em %s\n", nomes[i], nomes[j]);
        achou = true;
      }
      pinMode(pinos[i], INPUT);
    }
    if (!achou) Serial.println("[exame]   sem curto entre SDA e SCL");
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
  }

  Serial.println("[exame] veredito:");
  if (nBaixo)
    Serial.println("[exame]   RESPONDEU COM XSHUT EM BAIXO -> o XSHUT nao chega no modulo.");
  if (nTrocado && !nAlto && !nSolto && !nLento)
    Serial.println("[exame]   SO respondeu com os pinos trocados -> SDA e SCL estao invertidos:"
                   " SDA vai no 16, SCL no 17.");
  else if (nLento && !nAlto)
    Serial.println("[exame]   So respondeu a 100 kHz -> fio longo demais para 400 kHz.");
  else if (nSolto && !nAlto)
    Serial.println("[exame]   So respondeu com XSHUT SOLTO -> o fio de XSHUT esta partido ou"
                   " no pino errado: o nivel ALTO nao chega no modulo.");
  else if (nAlto)
    Serial.println("[exame]   Sensor responde normalmente. Se o boot falhou, e temporizacao.");
  else
    Serial.println("[exame]   NINGUEM respondeu em condicao nenhuma, mas ha pull-up nas linhas:"
                   " chegou alimentacao no modulo e nao chegou comunicacao. Confira se SDA e SCL"
                   " do modulo vao mesmo para 16 e 17, e se o GND e comum.");

  Serial.println("[exame] --- fim; reinicializando os olhos ---");
  diagBusy = false;
  tofBegin();
}

// ---------------------------------------------------------------------
static uint8_t bbSDA, bbSCL;

static inline void bbAlto(uint8_t pino) { pinMode(pino, INPUT_PULLUP); }
static inline void bbBaixo(uint8_t pino) { pinMode(pino, OUTPUT); digitalWrite(pino, LOW); }
static inline void bbEspera() { delayMicroseconds(5); }        // ~100 kHz

static void bbInicio() {
  bbAlto(bbSDA); bbAlto(bbSCL); bbEspera();
  bbBaixo(bbSDA); bbEspera();
  bbBaixo(bbSCL); bbEspera();
}

static void bbFim() {
  bbBaixo(bbSDA); bbEspera();
  bbAlto(bbSCL);  bbEspera();
  bbAlto(bbSDA);  bbEspera();
}

// Devolve true se o escravo puxou SDA para baixo (ACK).
static bool bbEscreveByte(uint8_t v) {
  for (uint8_t i = 0; i < 8; i++) {
    if (v & 0x80) bbAlto(bbSDA); else bbBaixo(bbSDA);
    v <<= 1;
    bbEspera();
    bbAlto(bbSCL); bbEspera();
    bbBaixo(bbSCL); bbEspera();
  }
  bbAlto(bbSDA); bbEspera();          // solta para o escravo responder
  bbAlto(bbSCL); bbEspera();
  bool ack = (digitalRead(bbSDA) == LOW);
  bbBaixo(bbSCL); bbEspera();
  return ack;
}

static uint8_t bbVarre(uint8_t sda, uint8_t scl, const char* nome) {
  bbSDA = sda; bbSCL = scl;
  bbAlto(bbSDA); bbAlto(bbSCL);
  delayMicroseconds(200);
  Serial.printf("[unha] varrendo %s (SDA %u / SCL %u):", nome, sda, scl);
  uint8_t achados = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bbInicio();
    bool ack = bbEscreveByte((uint8_t)(a << 1));
    bbFim();
    if (ack) { Serial.printf(" 0x%02X", a); achados++; }
    delayMicroseconds(50);
  }
  if (!achados) Serial.print("  NENHUM DISPOSITIVO");
  Serial.println();
  pinMode(sda, INPUT); pinMode(scl, INPUT);
  return achados;
}

void i2cNaUnha() {
  diagBusy = true;
  vTaskDelay(pdMS_TO_TICKS(40));
  Serial.println("[unha] --- I2C na unha, sem o periferico ---");

  Wire.end();

  uint8_t nBus  = bbVarre(PIN_SDA, PIN_SCL, "o barramento (21/22)");
  // Controle negativo: dois pinos que este projeto nao usa. Nada pode
  // responder ali. Se responder, o scanner na unha esta inventando ACK e
  // o resultado de cima nao vale nada.
  uint8_t nNada = bbVarre(5, 23, "GPIO 5 e 23, sem nada [controle -]");

  Wire.begin(PIN_SDA, PIN_SCL, 400000);

  Serial.println("[unha] veredito:");
  if (nNada)
    Serial.println("[unha]   o CONTROLE NEGATIVO respondeu: o scanner na unha esta"
                   " enxergando ACK onde nao ha ninguem. Ignore o resto.");
  else if (nBus)
    Serial.println("[unha]   achou na unha, sem o periferico -> os fios estao bons."
                   " Se a Wire nao acha, o problema e temporizacao ou o periferico.");
  else
    Serial.println("[unha]   controle limpo e barramento mudo ate na unha -> nao chega"
                   " comunicacao no modulo. Confira VCC, GND comum e os dois fios.");
  Serial.println("[unha] --- fim ---");
  diagBusy = false;
  tofBegin();
}

// ---------------------------------------------------------------------
void begin() {
  // Pull-up onde o pino tem: fio solto passa a repousar em ALTO ("seguro")
  // em vez de BAIXO ("borda"), e deixa de se disfarcar de leitura boa.
  // GPIO 34-39 nao tem pull-up interno - por isso o lado direito fica
  // dependendo do pull-up do proprio modulo.
  pinMode(PIN_IR_L_D, INPUT_PULLUP);
  pinMode(PIN_IR_R_D, INPUT);
  edgeSem = xSemaphoreCreateBinary();
  attachInterrupt(digitalPinToInterrupt(PIN_IR_L_D), isrIrL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IR_R_D), isrIrR, CHANGE);

  analogReadResolution(12);

  tofBegin();
}

} // namespace Sens
