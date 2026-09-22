// =====================================================================
//  ROBO SUMO v4 - config.h
//  Pinout, parametros ajustaveis e estruturas globais
//  Placa alvo: ESP32-C3 SuperMini (RISC-V, NUCLEO UNICO)
// =====================================================================
#pragma once
#include <Arduino.h>

// =====================================================================
//  AJUSTE RAPIDO
//
//  Os tres numeros que mais mudam na bancada. Estao aqui em cima, e nao
//  no painel web, de proposito: o valor que funcionou fica versionado,
//  com o comentario dizendo por que e aquele.
// =====================================================================

// Limiar do IR de borda, em MILIVOLTS lidos no AO.
//
//  Piso preto do dojo reflete pouco  -> fototransistor conduz pouco -> AO ALTO
//  Faixa branca (Tawara) reflete bem -> conduz muito               -> AO BAIXO
//
//  Entao "borda" e a leitura BAIXA. Calibrar assim: apertar 'p' no
//  monitor serial com o robo sobre o preto, 'b' sobre o branco, e por
//  este limiar no meio das duas medidas.
#define IR_LIMIAR_MV      1600
#define IR_BORDA_ABAIXO      1   // 1 = a faixa branca e a leitura MAIS BAIXA

// Ate onde um eco do HC-SR04 conta como oponente, em cm.
//  O dojo tem 77 cm de diametro: eco de 2 m e parede, e parede nao se
//  ataca. Diminuir se o robo estiver investindo contra o juiz.
#define US_ALCANCE_CM       60

// ---------------------------------------------------------------------
//  PINOUT
//
//  O C3 SuperMini expoe 13 GPIOs no header: 0..10, 20 e 21. Tres saem da
//  conta antes de qualquer coisa:
//
//    GPIO 2, 8, 9  -> strapping. O nivel deles no instante do reset
//                     decide de onde a placa da boot. Carregar um motor
//                     ou um sensor ali faz a placa nao subir, e o sintoma
//                     nao parece eletrico - parece firmware quebrado.
//                     O 8 ainda tem o LED azul da placa; o 9 e o BOOT.
//    GPIO 18, 19   -> USB nativo (D- e D+). Nem saem no header.
//
//  Sobram dez, e e com dez que este projeto fecha - com um de reserva.
//
//  O Serial vai por USB CDC, que e o que libera o par 20/21 (seriam a
//  UART0) para o ECHO e o receptor do edital.
// ---------------------------------------------------------------------

// ---- Motores / 1x L298N (modulo micro) ------------------------------
//
//    CANAL A -> roda ESQUERDA        CANAL B -> roda DIREITA
//    IN1 --.                         IN3 --.
//    IN2 --|                         IN4 --|
//          v                               v
//    OUT1 / OUT2 -> motor esquerdo   OUT3 / OUT4 -> motor direito
//
//  TABELA-VERDADE do L298N, por canal (datasheet ST, "Bridge Control"):
//
//     EN=L   qualquer IN      -> saidas em ALTA IMPEDANCIA (roda livre)
//     EN=H   IN1=H  IN2=L     -> gira num sentido
//     EN=H   IN1=L  IN2=H     -> gira no outro
//     EN=H   IN1=IN2          -> fast motor stop (FREIO)
//
//  DUAS CONSEQUENCIAS QUE MANDAM NO CODIGO:
//
//  1. O PWM vai nos pinos IN, nao no EN. Com EN fixo em ALTO e IN2
//     chaveando enquanto IN1 fica em ALTO, o canal alterna entre girar e
//     FREAR - que e decaimento lento, e da mais torque em baixa rotacao.
//     PWM no EN alternaria entre girar e roda livre (decaimento rapido),
//     que e justamente o que nao interessa num sumo: o que decide a
//     partida e o empurrao parado.
//
//  2. Com EN em ALTO NAO EXISTE roda livre. IN1=IN2 freia, seja em ALTO
//     ou em BAIXO. A unica forma de soltar o motor e baixar o EN - por
//     isso Mot::coast() mexe no EN, e nao so nos PWM.
//
//  ENA e ENB vao JUNTOS no mesmo GPIO: nunca foi preciso desligar um
//  lado sozinho, e amarrar os dois economiza um pino e deixa o robo
//  nascer com as saidas soltas, que e o estado seguro.
//
//  NIVEL LOGICO: o L298N pede Vih >= 2,3 V. A C3 entrega 3,3 V, entao os
//  comandos entram direto, sem level shifter. (A alimentacao logica do
//  CI e outra coisa: Vss quer 4,5 a 7 V - ver a secao de alimentacao no
//  README.)
//
//  PRECO A PAGAR: o L298N e Darlington bipolar e derruba cerca de 2 V na
//  propria ponte, mais sob carga. Dos 7,8 V da bateria o motor ve algo
//  perto de 5,8 V, e a diferenca vira calor no dissipador.
//
#define PIN_IN1          3   // canal A (esquerda) - recebe PWM
#define PIN_IN2          4   // canal A (esquerda) - recebe PWM
#define PIN_IN3          5   // canal B (direita)  - recebe PWM
#define PIN_IN4          6   // canal B (direita)  - recebe PWM
#define PIN_EN           7   // ENA + ENB juntos. ALTO = habilitado
                             // (nasce em BAIXO: saidas soltas = seguro)

// ---- Olho: 1x HC-SR04 (ultrassonico) --------------------------------
//
//  TRIG recebe um pulso de 10 us; o modulo dispara oito ciclos a 40 kHz
//  e levanta ECHO. A largura do ECHO e o tempo de ida e volta do som -
//  dividido por 58 da centimetros (nota de aplicacao, eq. 3).
//
//  TRIG aceita os 3,3 V da C3 direto. O ECHO NAO: ele sai em 5 V, e o
//  GPIO da C3 nao tolera isso. Vai por DIVISOR 1 k / 2 k:
//
//      ECHO --[ 1k ]--+--> GPIO 20
//                     |
//                   [ 2k ]
//                     |
//                    GND
//
//      Vout = 5 V x 2k / (1k + 2k) = 3,33 V     (maximo do GPIO: 3,6 V)
//
//  ECHO e lido por INTERRUPCAO, nao por pulseIn(). O pulseIn() e espera
//  ocupada e seguraria a CPU por ate 38 ms por leitura - numa placa de
//  nucleo unico isso disputaria tempo com a guarda de borda.
#define PIN_US_TRIG     10
#define PIN_US_ECHO     20

// Teto de espera do eco, em microssegundos. 25 ms ~ 4,3 m, que e o fim
// da escala do modulo: alem disso nao ha eco para esperar, so atraso.
#define US_TIMEOUT_US    25000UL
#define US_PERIODO_MS    60      // ~16 leituras/s; menos que isso e ouvir o proprio eco

// ---- Sensor IR de borda: SAIDA ANALOGICA (AO) -----------------------
//
//  Usamos o AO, e nao o DO. O DO ja vem comparado contra o trimpot do
//  modulo, entao o limiar mora num parafuso que ninguem consegue
//  versionar nem conferir. Lendo o AO, o limiar vira IR_LIMIAR_MV la em
//  cima: um numero no codigo, com historico no git.
//
//  GPIO 0 e ADC1_CH0. ADC1 de proposito - o ADC2 nao funciona com o
//  Wi-Fi ligado, e o ponto de acesso deste robo fica no ar.
//
//  Se o modulo for alimentado em 5 V, o AO chega a 5 V e precisa de
//  DIVISOR 10 k / 10 k:
//
//      AO --[ 10k ]--+--> GPIO 0
//                    |
//                 [ 10k ]
//                    |
//                   GND
//
//      Vout = 5 V x 10k / (10k + 10k) = 2,50 V
//
//  Por que 10k/10k aqui e 1k/2k no ECHO: o ECHO e digital e so precisa
//  caber embaixo de 3,6 V. Este e analogico, e o ADC da C3 satura perto
//  de 3,1 V - parar em 2,5 V mantem a escala inteira dentro da faixa
//  linear, em vez de achatar o preto contra o teto do conversor.
//
//  ATENCAO: o divisor divide TAMBEM o limiar. IR_LIMIAR_MV e o valor
//  medido NO GPIO, ja dividido - nao a tensao que sai do sensor.
#define PIN_IR_AO        0   // ADC1_CH0

// ---- Receptor do controle do edital (reservado) ---------------------
//
//  Pino mapeado e reservado. A decodificacao do protocolo entra depois:
//  por enquanto nada le este GPIO, e nenhuma biblioteca de IR e
//  compilada. Quem arma o robo hoje e o painel web ou a tecla 'a' da
//  serial.
#define PIN_RX_EDITAL   21

// GPIO 1 (ADC1_CH1) fica LIVRE - unica reserva, e ainda e pino de ADC.

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
  ST_IDLE = 0,     // parado, aguardando o start
  ST_COUNTDOWN,    // 5 s regulamentares
  ST_SEARCH,       // varredura procurando o oponente
  ST_LOCK,         // alvo detectado, confirmando (anti-ruido)
  ST_ATTACK,       // investida
  ST_EDGE,         // manobra de salvamento na borda
  ST_UNSTUCK,      // destravamento (empurrao sem progresso)
  ST_DANCE,        // dancinha da vitoria
  ST_MANUAL        // pilotagem pelo painel web
};

static const char* const STATE_NAME[] = {
  "IDLE", "COUNTDOWN", "BUSCA", "TRAVANDO", "ATAQUE",
  "BORDA!", "DESTRAVE", "DANCA", "MANUAL"
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
//  Nao sao editaveis pelo painel de proposito: a afinacao se faz no
//  codigo, conversando. O painel e instrumento de leitura.
// ---------------------------------------------------------------------
struct Params {
  uint16_t vSearch;       // 0..1000  velocidade de varredura
  uint16_t vAttack;       // 0..1000  velocidade de investida
  uint16_t vMax;          // 0..1000  teto absoluto
  uint16_t vReverse;      // 0..1000  velocidade de recuo na borda

  // Inverte o sentido de um lado por software: mais rapido do que trocar
  // os dois fios do motor no borne.
  uint8_t  motInvL, motInvR;

  uint16_t rangeCm;       // alcance util de deteccao
  uint8_t  confirmHits;   // leituras coerentes seguidas p/ confirmar alvo real
  uint8_t  loseMisses;    // leituras vazias seguidas p/ considerar alvo perdido
  uint16_t jumpCm;        // salto maximo entre leituras (acima disso = ruido)

  uint16_t edgeBackMs;    // tempo de recuo ao ver a borda
  uint16_t edgeTurnMs;    // tempo de giro apos o recuo

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
  /*vMax*/       900,   // o L298N ja tira ~2 V no caminho: menos teto a cortar
  /*vReverse*/   800,
  /*motInvL*/    0, /*motInvR*/ 0,
  /*rangeCm*/    US_ALCANCE_CM,
  /*confirmHits*/3,
  /*loseMisses*/ 6,
  /*jumpCm*/     35,
  /*edgeBackMs*/ 260,
  /*edgeTurnMs*/ 230,
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

  volatile uint16_t irMv;         // AO do IR, em mV JA DIVIDIDOS
  volatile bool     irBorda;      // decisao final: true = vendo a borda clara

  volatile int16_t  pwmL, pwmR;   // -1000..1000 aplicados

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
