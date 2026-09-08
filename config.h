// =====================================================================
//  ROBO SUMO - config.h
//  Pinout, parametros ajustaveis e estruturas globais
//  Placa alvo: ESP32 DevKit V1 (30 pinos)
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------
//  PINOUT  (agrupado por bloco fisico do header da DevKit V1)
//
//  Lateral A (VIN..EN):  13  12  14  27  26  25  33  32  35  34  39  36
//  Lateral B (D23..3V3): 23  22  TX  RX  21  19  18   5  17  16   4   2  15
// ---------------------------------------------------------------------

// ---- Motores / 2x DRV8833 (bloco 14-27-26-25 consecutivo) -----------
//
//  Uma ponte H por lado, com os DOIS canais em PARALELO dentro da placa:
//
//    PONTE H #1 (ESQUERDA)          PONTE H #2 (DIREITA)
//    IN1 = AIN1 + BIN1  --.         IN3 = AIN1 + BIN1  --.
//    IN2 = AIN2 + BIN2  --|         IN4 = AIN2 + BIN2  --|
//                         v                              v
//    OUT1 = AOUT1 + BOUT1           OUT3 = AOUT1 + BOUT1
//    OUT2 = AOUT2 + BOUT2           OUT4 = AOUT2 + BOUT2
//        |                              |
//        +-- motor frente esq           +-- motor frente dir
//        +-- motor tras   esq           +-- motor tras   dir
//
//  Paralelar entrada COM saida e um modo previsto pelo DRV8833 e dobra a
//  corrente do canal: 1,5 A RMS por canal viram ~3 A no par. Como os dois
//  motores de um lado dividem o mesmo par de saidas, essa folga importa -
//  dois N20 travados juntos passam de 1,5 A e desarmariam a protecao.
//
#define PIN_IN4         14   // Ponte H #2 (DIREITA)  - AIN2 + BIN2
#define PIN_IN3         27   // Ponte H #2 (DIREITA)  - AIN1 + BIN1
#define PIN_IN2         26   // Ponte H #1 (ESQUERDA) - AIN2 + BIN2
#define PIN_IN1         25   // Ponte H #1 (ESQUERDA) - AIN1 + BIN1
#define PIN_DRV_SLEEP   13   // nSLEEP das DUAS pontes (pull-down 10k = desligado no boot)
#define PIN_DRV_FAULT   15   // nFAULT das DUAS pontes (dreno aberto + pull-up 10k)

// ---- Sensores IR de borda (bloco 33-32-35-34 consecutivo) -----------
#define PIN_IR_R_A      33   // ADC1_CH5 - saida analogica IR direito  (calibracao)
#define PIN_IR_L_A      32   // ADC1_CH4 - saida analogica IR esquerdo (calibracao)
#define PIN_IR_R_D      35   // saida digital IR direito  (interrupcao - so entrada)
#define PIN_IR_L_D      34   // saida digital IR esquerdo (interrupcao - so entrada)

// ---- Olhos: 2x VL53L0X (Time-of-Flight, I2C) ------------------------
//  UM SO BARRAMENTO: os olhos foram para 21/22, junto com o OLED.
//
//  O desenho anterior dava barramento dedicado a eles (Wire1 em 16/17)
//  por um bom motivo: um quadro do OLED ocupa a linha por ~25 ms e isso
//  atrasaria a leitura de distancia justamente na investida. A separacao
//  caiu por um motivo de bancada, nao de projeto - o GPIO 17 estava
//  grampeado em 3,3 V e sem clock nao existe I2C. O preco esta pago em
//  face.h: o desenho e suspenso enquanto o robo ataca ou salva borda.
//
//  SDA/SCL sao COMPARTILHADOS pelos dois sensores; o que os separa e o
//  XSHUT, que permite ligar um de cada vez no boot e reendereçar.
#define PIN_TOF_SDA     21
#define PIN_TOF_SCL     22
#define PIN_TOF_L_XSHUT 19   // XSHUT e ativo em nivel BAIXO (datasheet Tab. 2)
#define PIN_TOF_R_XSHUT 18
#define TOF_ADDR_L      0x30 // reendereçado no boot
#define TOF_ADDR_R      0x29 // 0x52 de 8 bits = 0x29 de 7 bits (datasheet §3)

// Perfil "high speed" da Tab. 13: 20 ms de orcamento -> ~50 leituras/s
#define TOF_BUDGET_US   20000

// ---- I2C do OLED (Wire) - o MESMO barramento dos olhos --------------
#define PIN_SDA         21
#define PIN_SCL         22

// ---- Diversos -------------------------------------------------------
#define PIN_BUZZER       4   // buzzer passivo via NPN (2N2222) ou direto se piezo
// Tensao minima de pack para o robo armar sozinho no boot. Uma LiPo 2S
// descarregada ainda passa de 6 V; alimentado so pelo USB, o divisor le
// quase zero. E esse degrau que separa "estou na arena" de "estou na
// bancada" - ver o fim do setup() em RoboSumo.ino.
#define VBAT_ARMA_V   6.0f

// GPIO 23 ficou LIVRE: o botao de armar saiu do projeto. Quem arma e
// ligar a placa - ver o fim do setup() em RoboSumo.ino.
#define PIN_VBAT        39   // ADC1_CH3 - divisor 100k/47k da bateria
#define PIN_LED          2   // LED da placa

// ---- Canais LEDC (usados apenas no core Arduino 2.x) ----------------
#define CH_IN1     0
#define CH_IN2     1
#define CH_IN3     2
#define CH_IN4     3
#define CH_BUZZER  6

#define PWM_FREQ   20000     // 20 kHz: acima do audivel, motor nao "canta"
#define PWM_BITS   10
#define PWM_MAX    1023

// ---------------------------------------------------------------------
//  ESTADOS
// ---------------------------------------------------------------------
enum RoboState : uint8_t {
  ST_IDLE = 0,     // parado, aguardando ARM
  ST_COUNTDOWN,    // 5 s regulamentares
  ST_SEARCH,       // varredura procurando o oponente
  ST_LOCK,         // alvo detectado, confirmando (anti-ruido)
  ST_ATTACK,       // investida com PID
  ST_EDGE,         // manobra de salvamento na borda
  ST_UNSTUCK,      // destravamento (empurrao sem progresso)
  ST_DANCE,        // dancinha da vitoria
  ST_MANUAL,       // pilotagem pelo painel web
  ST_FAULT         // falha do driver / bateria critica
};

static const char* const STATE_NAME[] = {
  "IDLE", "COUNTDOWN", "BUSCA", "TRAVANDO", "ATAQUE",
  "BORDA!", "DESTRAVE", "DANCA", "MANUAL", "FALHA"
};

// ---------------------------------------------------------------------
//  MODOS
// ---------------------------------------------------------------------
enum RoboMode : uint8_t {
  MODE_NORMAL = 0,
  MODE_LESMA,       // devagar e sempre, nao se afoba
  MODE_CAPIROTO,    // extremo, no limite da logica
  MODE_CACADOR,     // busca agressiva de longo alcance
  MODE_MURALHA,     // defensivo: gira pouco, so reage
  MODE_DANCA,       // dancinha da vitoria
  MODE_COUNT
};

static const char* const MODE_NAME[] = {
  "NORMAL", "LESMA", "CAPIROTO", "CACADOR", "MURALHA", "DANCINHA"
};

// ---------------------------------------------------------------------
//  PARAMETROS AJUSTAVEIS (salvos na NVS, editaveis pelo painel web)
// ---------------------------------------------------------------------
struct Params {
  uint16_t vSearch;       // 0..1000  velocidade de varredura
  uint16_t vAttack;       // 0..1000  velocidade de investida
  uint16_t vMax;          // 0..1000  teto absoluto (protege motor 6V em bateria 7.4V)
  uint16_t vReverse;      // 0..1000  velocidade de recuo na borda

  // Inverte o sentido de um lado por software. Com dois motores no mesmo
  // par de saidas, se o lado inteiro girar ao contrario e mais rapido
  // corrigir aqui do que trocar quatro fios no borne.
  uint8_t  motInvL, motInvR;

  float    kp, ki, kd;    // PID de direcionamento (erro = distEsq - distDir)

  uint16_t rangeCm;       // alcance util de deteccao
  uint8_t  confirmHits;   // leituras coerentes seguidas p/ confirmar alvo real
  uint8_t  loseMisses;    // leituras vazias seguidas p/ considerar alvo perdido
  uint16_t jumpCm;        // salto maximo entre leituras (acima disso = ruido)

  uint16_t edgeBackMs;    // tempo de recuo ao ver a borda
  uint16_t edgeTurnMs;    // tempo de giro apos o recuo
  uint16_t irThreshold;   // limiar ADC claro/escuro
  uint8_t  irSource;      // 0 = so pino DO | 1 = so ADC (AO) | 2 = os dois
  uint8_t  irActiveLow;   // 1 = sinal indica borda com nivel BAIXO / ADC baixo

  uint16_t sweepMs;       // duracao de cada varredura antes de inverter o giro
  uint16_t rampMs;        // tempo de rampa 0 -> vMax (0 = sem rampa)
  uint16_t stuckMs;       // tempo empurrando sem variacao -> destravar
  uint16_t countdownMs;   // 5000 no regulamento

  uint8_t  mode;
  uint8_t  soundOn;
  uint8_t  faceOn;
  uint8_t  autoRestart;   // volta a buscar sozinho depois da danca
  uint16_t vbatMin;       // centesimos de volt (ex.: 660 = 6.60 V) -> corta motores
};

extern Params P;

// Padrao de fabrica
static const Params P_DEFAULT = {
  /*vSearch*/    420,
  /*vAttack*/    900,
  /*vMax*/       820,
  /*vReverse*/   750,
  /*motInvL*/    0, /*motInvR*/ 0,
  /*kp*/         6.0f,  /*ki*/ 0.05f, /*kd*/ 2.2f,
  // Tab. 11 da datasheet: alvo cinza 17% da ~80 cm indoor e cai sob luz
  // forte. Oponente de sumo costuma ser preto, entao 80 cm e o realista.
  /*rangeCm*/    80,
  /*confirmHits*/3,
  /*loseMisses*/ 6,
  /*jumpCm*/     35,
  /*edgeBackMs*/ 260,
  /*edgeTurnMs*/ 230,
  /*irThreshold*/1800,
  // Padrao no ADC: o AO da um valor que da para conferir e calibrar. O DO
  // depende de o pino estar mesmo ligado, e GPIO34/35 nao tem pull-up
  // interno - solto, ficam em BAIXO e pareceriam borda para sempre.
  /*irSource*/   0,   // 3 fios: so o pino digital existe
  /*irActiveLow*/1,
  /*sweepMs*/    900,
  /*rampMs*/     140,
  /*stuckMs*/    1400,
  /*countdownMs*/5000,
  /*mode*/       MODE_NORMAL,
  /*soundOn*/    1,
  /*faceOn*/     1,
  /*autoRestart*/1,
  /*vbatMin*/    660
};

// ---------------------------------------------------------------------
//  TELEMETRIA (o que o painel web mostra em tempo real)
// ---------------------------------------------------------------------
struct Telemetry {
  volatile uint8_t  state;
  volatile uint8_t  mode;
  volatile bool     armed;

  volatile int16_t  distL;        // cm, -1 = sem eco
  volatile int16_t  distR;
  volatile int16_t  distFused;    // menor distancia valida
  volatile int8_t   bearing;      // -1 esquerda, 0 centro, +1 direita
  volatile uint8_t  confidence;   // 0..100 - confianca de que o alvo e real

  volatile uint16_t irLraw, irRraw;   // ADC cru dos pinos AO
  volatile bool     irLdo, irRdo;     // estado cru dos pinos DO
  volatile bool     irL, irR;         // decisao final: true = vendo a borda clara
  volatile int16_t  tofRawL, tofRawR;   // ultima leitura crua em mm (-1 = fora de alcance)
  volatile uint16_t tofFailL, tofFailR; // leituras invalidas seguidas
  volatile bool     tofOkL, tofOkR;     // sensor inicializado e medindo
  volatile bool     tofNoBus;           // algo responde no barramento dos olhos

  volatile int16_t  pwmL, pwmR;   // -1000..1000 aplicados
  volatile float    pidP, pidI, pidD, pidOut;

  volatile uint16_t vbat;         // centesimos de volt
  volatile bool     drvFault;
  volatile bool     oledOk;       // display respondeu no I2C
  volatile uint8_t  apClients;    // celulares conectados no AP
  volatile uint32_t uptimeMs;
  volatile uint32_t countdownLeft;

  volatile uint16_t nAttacks;
  volatile uint16_t nEdgeSaves;
  volatile uint16_t nLost;
  volatile uint32_t timeAttackingMs;
  volatile uint16_t loopHz;
};

extern Telemetry T;

// ---------------------------------------------------------------------
//  REGISTRO DE COMBATE (anel de eventos exibido no painel)
// ---------------------------------------------------------------------
enum EvtType : uint8_t { EV_ARM=0, EV_LOCK, EV_ATTACK, EV_EDGE, EV_LOST, EV_STUCK, EV_WIN, EV_FAULT };
static const char* const EVT_NAME[] = { "ARM", "LOCK", "ATAQUE", "BORDA", "PERDEU", "TRAVOU", "VITORIA", "FALHA" };

struct Event {
  uint32_t t;        // ms desde o ARM
  uint8_t  type;
  int16_t  a;        // distancia / lado / etc
  int16_t  b;        // duracao / pwm / etc
};

#define EVT_MAX 48
extern Event  EVLOG[EVT_MAX];
extern uint8_t EVN;      // quantos validos
extern uint8_t EVI;      // proximo indice de escrita

void evPush(uint8_t type, int16_t a, int16_t b);

// ---------------------------------------------------------------------
//  Compatibilidade LEDC entre core Arduino-ESP32 2.x e 3.x
// ---------------------------------------------------------------------
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  static inline void pwmSetup(uint8_t pin, uint8_t ch, uint32_t f, uint8_t bits) {
    (void)ch; ledcAttach(pin, f, bits);
  }
  static inline void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
    (void)ch; ledcWrite(pin, duty);
  }
  static inline void pwmTone(uint8_t pin, uint8_t ch, uint32_t freq) {
    (void)ch;
    if (freq) ledcWriteTone(pin, freq);
    else      ledcWrite(pin, 0);
  }
#else
  static inline void pwmSetup(uint8_t pin, uint8_t ch, uint32_t f, uint8_t bits) {
    ledcSetup(ch, f, bits); ledcAttachPin(pin, ch);
  }
  static inline void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
    (void)pin; ledcWrite(ch, duty);
  }
  static inline void pwmTone(uint8_t pin, uint8_t ch, uint32_t freq) {
    (void)pin;
    if (freq) ledcWriteTone(ch, freq);
    else      ledcWrite(ch, 0);
  }
#endif

static inline int16_t clampi(int32_t v, int16_t lo, int16_t hi) {
  return (int16_t)(v < lo ? lo : (v > hi ? hi : v));
}
