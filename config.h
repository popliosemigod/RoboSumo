// =====================================================================
//  ROBO SUMO v2 - config.h
//  Pinout, parametros ajustaveis e estruturas globais
//  Placa alvo: ESP32-C3 Mini / SuperMini (RISC-V, NUCLEO UNICO)
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------
//  PINOUT
//
//  O C3 Mini expoe 13 GPIOs no header: 0..10, 20 e 21. Tres deles saem
//  da conta antes de qualquer coisa:
//
//    GPIO 2, 8, 9  -> strapping. O nivel deles no instante do reset
//                     decide de onde a placa da boot. Carregar um motor
//                     ou um sensor ali faz a placa nao subir, e o sintoma
//                     nao parece eletrico - parece firmware quebrado.
//                     O 8 ainda tem o LED azul da placa; o 9 e o botao
//                     BOOT.
//    GPIO 18, 19   -> USB nativo (D- e D+). Nem saem no header.
//
//  Sobram exatamente dez: 0, 1, 3, 4, 5, 6, 7, 10, 20, 21 - e este
//  projeto usa os dez. O Serial vai por USB CDC, que e o que libera o
//  par 20/21 (seriam a UART0) para o receptor do juiz e o nFAULT.
//
//  Nenhum sensor deste robo usa ADC. Isso apaga de vez a armadilha do
//  ADC2, que nao funciona com o Wi-Fi ligado.
// ---------------------------------------------------------------------

// ---- Motores / 2x DRV8833 -------------------------------------------
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
//  E por causa desse paralelo que duas pontes cabem em QUATRO pinos, e
//  nao em doze. Paralelar entrada com saida e um modo previsto pelo
//  DRV8833 (datasheet, "Parallel Mode") e dobra a corrente do canal:
//  1,5 A RMS viram ~3 A no par. Como os dois motores de um lado dividem
//  o mesmo par de saidas, essa folga importa - dois N20 travados juntos
//  passam de 1,5 A e desarmariam a protecao de sobrecorrente.
//
//  Efeito colateral util: as duas rodas de um lado nunca discordam,
//  porque recebem literalmente a mesma tensao.
//
#define PIN_IN1          0   // Ponte H #1 (ESQUERDA) - AIN1 + BIN1
#define PIN_IN2          1   // Ponte H #1 (ESQUERDA) - AIN2 + BIN2
#define PIN_IN3          3   // Ponte H #2 (DIREITA)  - AIN1 + BIN1
#define PIN_IN4          4   // Ponte H #2 (DIREITA)  - AIN2 + BIN2
#define PIN_DRV_SLEEP    5   // nSLEEP das DUAS pontes (pull-down 10k = nasce dormindo)
#define PIN_DRV_FAULT   21   // nFAULT das DUAS pontes (dreno aberto + pull-up 10k)

// ---- Olho: 1x HC-SR04 (ultrassonico) --------------------------------
//
//  TRIG recebe um pulso de 10 us; o modulo dispara oito ciclos a 40 kHz
//  e levanta ECHO. A largura do pulso de ECHO e o tempo de ida e volta
//  do som - dividido por 58 da centimetros (datasheet, eq. 3).
//
//  ECHO e lido por INTERRUPCAO, nao por pulseIn(). O pulseIn() e espera
//  ocupada: numa placa de nucleo unico ele seguraria a CPU por ate 38 ms
//  a cada leitura, e 38 ms e tempo de sobra para o robo passar da borda.
//
//  Alimentado em 3,3 V nesta montagem. O modulo e especificado para 5 V
//  e vai render menos alcance, mas em troca o ECHO sai em 3,3 V e entra
//  direto no GPIO. Se um dia ele for para 5 V, ECHO passa a precisar de
//  divisor - e ai o divisor entra nesta tabela, nao antes.
#define PIN_US_TRIG      6
#define PIN_US_ECHO      7

// Teto de espera do eco, em microssegundos. 25 ms ~ 4,3 m, que e o fim
// da escala do modulo: alem disso nao ha eco para esperar, so atraso.
#define US_TIMEOUT_US    25000UL
#define US_PERIODO_MS    60      // ~16 leituras/s; menos que isso e ouvir o proprio eco

// ---- Sensor IR de borda ---------------------------------------------
//
//  UM modulo so, montado o mais a frente possivel. Na pratica o robo
//  passa a enxergar a borda apenas quando o nariz ja esta sobre ela,
//  entao a manobra de fuga e sempre "recua e gira", nunca "gira para o
//  lado que nao viu" - nao ha lado que nao viu.
//
//  GPIO 10 tem pull-up interno. Isso nao e detalhe: fio solto passa a
//  repousar em ALTO ("seguro") em vez de BAIXO ("borda"). Num pino sem
//  pull-up, jumper mal encaixado paralisa o robo em manobra de borda
//  eterna e se disfarca de leitura legitima - ja aconteceu neste projeto
//  e custou uma troca de modulos ate aparecer.
#define PIN_IR_BORDA    10

// ---- Receptor IR do juiz (Artigo 18 do regulamento) -----------------
//
//  Obrigatorio: receptor de 950 nm sintonizado em 38 kHz, para os
//  comandos Ready / Start / Stop do controle da RoboCore. Fica na parte
//  de cima do robo, com vista livre. Saida em dreno aberto, repousa ALTO.
#define PIN_IR_JUIZ     20

// ---- PWM ------------------------------------------------------------
//  O C3 tem 6 canais LEDC; usamos 4. No core 3.x o canal e escolhido
//  pela propria biblioteca a partir do pino, e CH_* vira so rotulo.
#define CH_IN1     0
#define CH_IN2     1
#define CH_IN3     2
#define CH_IN4     3

#define PWM_FREQ   20000     // 20 kHz: acima do audivel, motor nao "canta"
#define PWM_BITS   10
#define PWM_MAX    1023

// ---------------------------------------------------------------------
//  ESTADOS
// ---------------------------------------------------------------------
enum RoboState : uint8_t {
  ST_IDLE = 0,     // parado, aguardando o juiz
  ST_COUNTDOWN,    // 5 s regulamentares
  ST_SEARCH,       // varredura procurando o oponente
  ST_LOCK,         // alvo detectado, confirmando (anti-ruido)
  ST_ATTACK,       // investida
  ST_EDGE,         // manobra de salvamento na borda
  ST_UNSTUCK,      // destravamento (empurrao sem progresso)
  ST_DANCE,        // dancinha da vitoria
  ST_MANUAL,       // pilotagem pelo painel web
  ST_FAULT         // falha do driver
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
//  PARAMETROS AJUSTAVEIS (salvos na NVS)
//
//  Deixaram de ser editaveis pelo painel de proposito. A afinacao agora
//  se faz no codigo, conversando - o painel virou instrumento de
//  leitura, nao de ajuste.
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

  uint16_t rangeCm;       // alcance util de deteccao
  uint8_t  confirmHits;   // leituras coerentes seguidas p/ confirmar alvo real
  uint8_t  loseMisses;    // leituras vazias seguidas p/ considerar alvo perdido
  uint16_t jumpCm;        // salto maximo entre leituras (acima disso = ruido)

  uint16_t edgeBackMs;    // tempo de recuo ao ver a borda
  uint16_t edgeTurnMs;    // tempo de giro apos o recuo
  uint8_t  irActiveLow;   // 1 = borda indicada por nivel BAIXO no pino

  uint16_t sweepMs;       // duracao de cada varredura antes de inverter o giro
  uint16_t rampMs;        // tempo de rampa 0 -> vMax (0 = sem rampa)
  uint16_t stuckMs;       // tempo empurrando sem variacao -> destravar
  uint16_t countdownMs;   // 5000 no regulamento

  uint8_t  mode;
  uint8_t  autoRestart;   // volta a buscar sozinho depois da danca
};

extern Params P;

// Padrao de fabrica
static const Params P_DEFAULT = {
  /*vSearch*/    420,
  /*vAttack*/    900,
  /*vMax*/       820,
  /*vReverse*/   750,
  /*motInvL*/    0, /*motInvR*/ 0,
  // HC-SR04 alcanca 4 m, mas o dojo tem 77 cm: alvo alem de ~80 cm esta
  // fora da arena e so pode ser parede, juiz ou perna de mesa.
  /*rangeCm*/    80,
  /*confirmHits*/3,
  /*loseMisses*/ 6,
  /*jumpCm*/     35,
  /*edgeBackMs*/ 260,
  /*edgeTurnMs*/ 230,
  /*irActiveLow*/1,
  /*sweepMs*/    900,
  /*rampMs*/     140,
  /*stuckMs*/    1400,
  /*countdownMs*/5000,
  /*mode*/       MODE_NORMAL,
  /*autoRestart*/1
};

// ---------------------------------------------------------------------
//  TELEMETRIA (o que o painel web mostra em tempo real)
// ---------------------------------------------------------------------
struct Telemetry {
  volatile uint8_t  state;
  volatile uint8_t  mode;
  volatile bool     armed;

  volatile int16_t  dist;         // cm, -1 = sem eco valido
  volatile uint16_t echoUs;       // largura crua do ECHO, em us
  volatile uint16_t usFail;       // leituras invalidas seguidas
  volatile uint8_t  confidence;   // 0..100 - confianca de que o alvo e real

  volatile bool     irDo;         // nivel cru do pino do IR
  volatile bool     irBorda;      // decisao final: true = vendo a borda clara

  volatile int16_t  pwmL, pwmR;   // -1000..1000 aplicados

  volatile bool     drvFault;
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
//  Compatibilidade LEDC entre core Arduino-ESP32 2.x e 3.x
// ---------------------------------------------------------------------
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  static inline void pwmSetup(uint8_t pin, uint8_t ch, uint32_t f, uint8_t bits) {
    (void)ch; ledcAttach(pin, f, bits);
  }
  static inline void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
    (void)ch; ledcWrite(pin, duty);
  }
#else
  static inline void pwmSetup(uint8_t pin, uint8_t ch, uint32_t f, uint8_t bits) {
    ledcSetup(ch, f, bits); ledcAttachPin(pin, ch);
  }
  static inline void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
    (void)pin; ledcWrite(ch, duty);
  }
#endif

static inline int16_t clampi(int32_t v, int16_t lo, int16_t hi) {
  return (int16_t)(v < lo ? lo : (v > hi ? hi : v));
}
