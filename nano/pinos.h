// =====================================================================
//  ROBO SUMO - porte para Arduino Nano (ATmega328P) + shield NANO Pro
//  pinos.h - pinout e o PORQUE de cada escolha
//
//  O Nano nao e um ESP32 menor: ele impoe restricoes que mudam o
//  desenho. As tres que mais pesam aqui:
//
//  1. UM SO BARRAMENTO I2C (A4/A5). No ESP32 o OLED tinha barramento
//     proprio justamente porque um quadro do display ocupa a linha por
//     ~25 ms e isso atrasaria a leitura de distancia no meio da
//     investida. No Nano nao ha como separar: OLED e os dois VL53L0X
//     dividem A4/A5. O firmware compensa nao desenhando enquanto ataca.
//
//  2. TIMERS COMPARTILHADOS. analogWrite() usa o timer do pino:
//     D5/D6 -> Timer0 (o mesmo do millis: NAO reconfigurar),
//     D9/D10 -> Timer1, D3/D11 -> Timer2. E tone() sequestra o Timer2.
//     Por isso os quatro pinos de motor ficam em Timer0 e Timer1, e o
//     buzzer fica sozinho no Timer2.
//
//  3. LOGICA DE 5 V. O VL53L0X aguenta 3,6 V no maximo (Tab. 6 da
//     datasheet). Ver o aviso em cima do bloco dos olhos.
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------
//  MOTORES - 2x DRV8833, um por lado, canais em paralelo
//
//  Os quatro pinos precisam de PWM porque o sentido decide qual dos dois
//  entra em PWM (Tab. 3 da datasheet do DRV8833: em decaimento lento um
//  pino fica em ALTO e o outro recebe o PWM).
//
//  ESQUERDA em Timer0 (976 Hz) e DIREITA em Timer1 (490 Hz). As duas
//  frequencias sao diferentes de proposito: Timer0 e o do millis() e
//  nao pode ser mexido. Duty igual continua dando tensao media igual,
//  entao os lados andam juntos - o que muda e so o ruido audivel.
//  Regressao conhecida: no ESP32 isso rodava a 20 kHz, inaudivel.
// ---------------------------------------------------------------------
#define PIN_IN1          5   // ponte ESQUERDA - AIN1+BIN1   (PWM Timer0)
#define PIN_IN2          6   // ponte ESQUERDA - AIN2+BIN2   (PWM Timer0)
#define PIN_IN3          9   // ponte DIREITA  - AIN1+BIN1   (PWM Timer1)
#define PIN_IN4         10   // ponte DIREITA  - AIN2+BIN2   (PWM Timer1)
#define PIN_DRV_SLEEP    7   // nSLEEP das DUAS pontes (pull-down 10k)
#define PIN_DRV_FAULT    8   // nFAULT das DUAS pontes (dreno aberto + pull-up 10k)

// ---------------------------------------------------------------------
//  IR DE BORDA - o caminho que salva o robo
//
//  D2 e a unica interrupcao externa que sobrou livre (INT0); o lado
//  direito usa interrupcao por mudanca de pino (PCINT20) em D4. As duas
//  reagem em microssegundos - a diferenca e so que a PCINT compartilha
//  vetor, e aqui nao ha mais ninguem naquele grupo.
//
//  Ao contrario do ESP32, estes pinos TEM pull-up interno. Pino solto
//  aqui le ALTO, nao BAIXO: o sintoma de fio solto e o oposto do que
//  era na outra placa.
// ---------------------------------------------------------------------
#define PIN_IR_L_D       2   // DO esquerdo - INT0
#define PIN_IR_R_D       4   // DO direito  - PCINT20
#define PIN_IR_L_A      A0   // AO esquerdo - confirmacao e calibracao
#define PIN_IR_R_A      A1   // AO direito

// ---------------------------------------------------------------------
//  OLHOS - 2x VL53L0X no MESMO barramento do OLED
//
//  !! O Nano fala I2C em 5 V e o VL53L0X aguenta 3,6 V no maximo em
//     SDA, SCL e XSHUT (Tab. 6). Alimentar o modulo GY-530 em 5 V faz
//     o conversor de nivel dele referenciar 5 V e resolve - mas isso so
//     vale para modulo COM conversor. Modulo pelado morre. Ver o README
//     desta pasta antes de energizar.
//
//  XSHUT continua sendo o que separa os dois: nascem ambos em 0x29 e o
//  boot acorda um de cada vez para reendereçar o esquerdo em 0x30.
// ---------------------------------------------------------------------
#define PIN_TOF_L_XSHUT 11   // XSHUT olho esquerdo
#define PIN_TOF_R_XSHUT 12   // XSHUT olho direito
#define TOF_ADDR_L    0x30   // reendereçado no boot
#define TOF_ADDR_R    0x29   // padrao de fabrica
#define TOF_BUDGET_US 20000  // perfil "high speed" da Tab. 13

// ---------------------------------------------------------------------
//  DIVERSOS
// ---------------------------------------------------------------------
#define PIN_BUZZER       3   // tone() -> Timer2, sozinho nele
#define PIN_LED         13   // LED de bordo: pisca quando armado
// A3 ficou livre: o botao de armar saiu do projeto inteiro.
#define PIN_VBAT        A2   // divisor 100k/47k da bateria

// A4 = SDA e A5 = SCL sao fixos no ATmega328P.
// A6 e A7 ficam livres (so entrada analogica, sem funcao digital).
//
// Aviso do shield: no NANO Pro Shield o conector rotulado SDA/SCL NAO
// funciona ("此版本SDA SCL无效" no esquematico dele). Use os pinos
// A4 e A5 dos blocos G/V/S de analogico.

#define PWM_MAX 255          // AVR: analogWrite e de 8 bits

// ---------------------------------------------------------------------
//  ESTADOS E MODOS
// ---------------------------------------------------------------------
enum RoboState : uint8_t {
  ST_IDLE = 0, ST_COUNTDOWN, ST_SEARCH, ST_LOCK,
  ST_ATTACK, ST_EDGE, ST_UNSTUCK, ST_MANUAL, ST_FAULT
};

enum RoboMode : uint8_t { MODE_NORMAL = 0, MODE_LESMA, MODE_CAPIROTO, MODE_COUNT };

static inline int16_t clampi(int32_t v, int16_t lo, int16_t hi) {
  return (int16_t)(v < lo ? lo : (v > hi ? hi : v));
}
