// =====================================================================
//  ROBO SUMO - motors.h
//
//  Duas DRV8833, uma por lado, em decaimento lento (slow decay).
//  Cada ponte tem os dois canais em paralelo (AIN||BIN e AOUT||BOUT),
//  entao 2 GPIOs comandam um par de saidas capaz de ~3 A, e os dois
//  motores daquele lado penduram nessas mesmas saidas:
//
//     IN1/IN2 -> OUT1/OUT2 -> motor frente esq + motor tras esq
//     IN3/IN4 -> OUT3/OUT4 -> motor frente dir + motor tras dir
//
//  Consequencia util: as duas rodas de um lado nunca discordam, porque
//  recebem literalmente a mesma tensao.
// =====================================================================
#pragma once
#include "config.h"

namespace Mot {

int16_t curL = 0, curR = 0;      // velocidade aplicada agora (-1000..1000)
int16_t tgtL = 0, tgtR = 0;      // velocidade desejada
bool    sleeping = true;
uint32_t lastRamp = 0;

// ---- escrita crua num par de saidas ---------------------------------
// speed: -1000..1000 | brake=true trava as rodas (freio eletrico)
static void raw(uint8_t pinA, uint8_t chA, uint8_t pinB, uint8_t chB,
                int16_t speed, bool brake) {
  if (speed == 0) {
    if (brake) { pwmWrite(pinA, chA, PWM_MAX); pwmWrite(pinB, chB, PWM_MAX); } // freio
    else       { pwmWrite(pinA, chA, 0);       pwmWrite(pinB, chB, 0); }       // livre
    return;
  }
  uint32_t duty = (uint32_t)abs(speed) * PWM_MAX / 1000;
  if (duty > PWM_MAX) duty = PWM_MAX;
  // Slow decay: um pino em 100%, o outro em (100% - duty).
  // Da muito mais torque em baixa rotacao que o fast decay.
  if (speed > 0) { pwmWrite(pinA, chA, PWM_MAX);        pwmWrite(pinB, chB, PWM_MAX - duty); }
  else           { pwmWrite(pinA, chA, PWM_MAX - duty); pwmWrite(pinB, chB, PWM_MAX); }
}

// Manda para as pontes ja aplicando a inversao por lado.
static inline void driveSides(int16_t l, int16_t r, bool brake) {
  raw(PIN_IN1, CH_IN1, PIN_IN2, CH_IN2, P.motInvL ? (int16_t)-l : l, brake);
  raw(PIN_IN3, CH_IN3, PIN_IN4, CH_IN4, P.motInvR ? (int16_t)-r : r, brake);
}

inline void wake(bool on) {
  sleeping = !on;
  digitalWrite(PIN_DRV_SLEEP, on ? HIGH : LOW);
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

inline void stop(bool brake = true) { applyNow(0, 0, brake); }

inline void coast() { applyNow(0, 0, false); }

// Giro no proprio eixo. v>0 = horario (para a direita)
inline void spin(int16_t v) { set(v, -v); }
inline void spinNow(int16_t v) { applyNow(v, -v); }

inline void forward(int16_t v) { set(v, v); }

void begin() {
  pinMode(PIN_DRV_SLEEP, OUTPUT);
  digitalWrite(PIN_DRV_SLEEP, LOW);          // nasce dormindo = seguro
  pinMode(PIN_DRV_FAULT, INPUT_PULLUP);

  pwmSetup(PIN_IN1, CH_IN1, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN2, CH_IN2, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN3, CH_IN3, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_IN4, CH_IN4, PWM_FREQ, PWM_BITS);
  applyNow(0, 0, false);
  wake(false);
}

inline bool fault() { return digitalRead(PIN_DRV_FAULT) == LOW; }

} // namespace Mot
