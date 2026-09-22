// =====================================================================
//  ROBO SUMO v4 - motors.h
//
//  UM L298N (modulo micro), dois motores: canal A na roda esquerda,
//  canal B na direita.
//
//     IN1/IN2 -> OUT1/OUT2 -> motor esquerdo
//     IN3/IN4 -> OUT3/OUT4 -> motor direito
//     ENA + ENB amarrados no mesmo GPIO (PIN_EN)
//
//  DECAIMENTO LENTO, com o PWM nos pinos IN e o EN fixo em ALTO. Pela
//  tabela-verdade do L298N, com EN em ALTO:
//
//     IN1=H  IN2=L  -> gira            IN1=IN2 -> fast motor stop (freio)
//     IN1=L  IN2=H  -> gira ao contrario
//
//  Entao segurar IN1 em ALTO e chavear IN2 alterna girar <-> frear, que
//  e decaimento lento: mais torque em baixa rotacao do que o decaimento
//  rapido. Num sumo o que decide a partida e o empurrao parado, nao a
//  velocidade de ponta - por isso o PWM nao vai no EN, que alternaria
//  entre girar e roda livre.
//
//  E POR ISSO QUE coast() MEXE NO EN. Com EN em ALTO o L298N nao tem
//  roda livre nenhuma: IN1=IN2 freia, seja em ALTO ou em BAIXO. A unica
//  forma de soltar o motor e desabilitar a ponte.
// =====================================================================
#pragma once
#include "config.h"

namespace Mot {

int16_t curL = 0, curR = 0;      // velocidade aplicada agora (-1000..1000)
int16_t tgtL = 0, tgtR = 0;      // velocidade desejada
bool    sleeping = true;
uint32_t lastRamp = 0;

// ---- escrita crua num canal -----------------------------------------
// speed: -1000..1000 | brake=true trava a roda (freio eletrico)
//
// Atencao: com EN em ALTO, "speed 0 sem freio" NAO solta a roda - o
// L298N freia de qualquer jeito. Quem solta e coast(), baixando o EN.
static void raw(uint8_t pinA, uint8_t chA, uint8_t pinB, uint8_t chB,
                int16_t speed, bool brake) {
  if (speed == 0) {
    if (brake) { pwmWrite(pinA, chA, PWM_MAX); pwmWrite(pinB, chB, PWM_MAX); } // H,H
    else       { pwmWrite(pinA, chA, 0);       pwmWrite(pinB, chB, 0); }       // L,L
    return;
  }
  uint32_t duty = (uint32_t)abs(speed) * PWM_MAX / 1000;
  if (duty > PWM_MAX) duty = PWM_MAX;
  // Um pino em 100%, o outro em (100% - duty): alterna entre girar e
  // frear dentro do proprio ciclo de PWM.
  if (speed > 0) { pwmWrite(pinA, chA, PWM_MAX);        pwmWrite(pinB, chB, PWM_MAX - duty); }
  else           { pwmWrite(pinA, chA, PWM_MAX - duty); pwmWrite(pinB, chB, PWM_MAX); }
}

// Manda para a ponte ja aplicando a inversao por lado.
static inline void driveSides(int16_t l, int16_t r, bool brake) {
  raw(PIN_IN1, CH_IN1, PIN_IN2, CH_IN2, P.motInvL ? (int16_t)-l : l, brake);
  raw(PIN_IN3, CH_IN3, PIN_IN4, CH_IN4, P.motInvR ? (int16_t)-r : r, brake);
}

// Habilita ou desabilita as DUAS pontes. Com EN em BAIXO as saidas ficam
// em alta impedancia: e o unico estado de roda livre que o L298N tem.
inline void wake(bool on) {
  sleeping = !on;
  digitalWrite(PIN_EN, on ? HIGH : LOW);
}

// Aplica imediatamente, sem rampa (usado pelo guarda de borda)
void applyNow(int16_t l, int16_t r, bool brake = false) {
  l = clampi(l, -1000, 1000);
  r = clampi(r, -1000, 1000);
  int16_t lim = (int16_t)P.vMax;
  if (l >  lim) l =  lim; if (l < -lim) l = -lim;
  if (r >  lim) r =  lim; if (r < -lim) r = -lim;
  curL = tgtL = l;
  curR = tgtR = r;
  if ((l || r) && sleeping) wake(true);
  driveSides(l, r, brake);
  T.pwmL = l; T.pwmR = r;        // telemetria mostra o comando logico
}

// Alvo com rampa suave (evita cavalo-de-pau e picos de corrente)
inline void set(int16_t l, int16_t r) {
  tgtL = clampi(l, -1000, 1000);
  tgtR = clampi(r, -1000, 1000);
}

void update() {
  uint32_t now = millis();
  uint32_t dt = now - lastRamp;
  if (dt < 5) return;
  lastRamp = now;

  int16_t step = P.rampMs ? (int16_t)((1000L * (int32_t)dt) / P.rampMs) : 1000;
  if (step < 1) step = 1;

  // Frear/inverter e sempre imediato: reacao vale mais que suavidade
  if ((tgtL >= 0) != (curL >= 0) || abs(tgtL) < abs(curL)) curL = tgtL;
  else curL += clampi(tgtL - curL, -step, step);

  if ((tgtR >= 0) != (curR >= 0) || abs(tgtR) < abs(curR)) curR = tgtR;
  else curR += clampi(tgtR - curR, -step, step);

  int16_t lim = (int16_t)P.vMax;
  int16_t l = clampi(curL, -lim, lim);
  int16_t r = clampi(curR, -lim, lim);

  if ((l || r) && sleeping) wake(true);
  driveSides(l, r, false);
  T.pwmL = l; T.pwmR = r;
}

// ---- comandos de alto nivel, usados pelo painel web -----------------
inline void stop(bool brake = true) { applyNow(0, 0, brake); }

// Roda livre de verdade: zera os PWM E desabilita a ponte. So zerar os
// PWM deixaria o L298N freando.
inline void coast() {
  driveSides(0, 0, false);
  curL = curR = tgtL = tgtR = 0;
  T.pwmL = T.pwmR = 0;
  wake(false);
}

// Giro no proprio eixo. v>0 = horario (para a direita)
inline void spin(int16_t v)     { set(v, -v); }
inline void spinNow(int16_t v)  { applyNow(v, -v); }

inline void forward(int16_t v)  { set(v, v); }
inline void back(int16_t v)     { set(-v, -v); }

void begin() {
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);            // nasce desabilitado = seguro

  pwmSetup(PIN_IN1, CH_IN1, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN2, CH_IN2, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN3, CH_IN3, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN4, CH_IN4, PWM_FREQ, PWM_BITS);
  driveSides(0, 0, false);
  wake(false);
}

} // namespace Mot
