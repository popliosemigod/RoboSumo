// =====================================================================
//  ROBO SUMO v3 - motors.h
//
//  UMA TB6612FNG, dois motores: canal A na roda esquerda, canal B na
//  direita.
//
//     AIN1/AIN2 -> AO1/AO2 -> motor esquerdo
//     BIN1/BIN2 -> BO1/BO2 -> motor direito
//
//  Decaimento LENTO (slow decay), com PWMA/PWMB jumpeados em 3V3 e o
//  PWM aplicado nos proprios AIN/BIN. Pela tabela-verdade da datasheet
//  (pg. 4), com PWM em ALTO:
//
//     IN1=H  IN2=L  -> CW            IN1=H  IN2=H -> short brake
//     IN1=L  IN2=H  -> CCW           IN1=L  IN2=L -> stop (alta impedancia)
//
//  Entao segurar IN1 em ALTO e chavear IN2 alterna CW <-> short brake,
//  que e exatamente decaimento lento. Da muito mais torque em baixa
//  rotacao que o decaimento rapido - e num sumo o que decide a partida
//  e justamente o empurrao parado, nao a velocidade de ponta.
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
  raw(PIN_AIN1, CH_AIN1, PIN_AIN2, CH_AIN2, P.motInvL ? (int16_t)-l : l, brake);
  raw(PIN_BIN1, CH_BIN1, PIN_BIN2, CH_BIN2, P.motInvR ? (int16_t)-r : r, brake);
}

inline void wake(bool on) {
  sleeping = !on;
  digitalWrite(PIN_STBY, on ? HIGH : LOW);
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
inline void coast() { applyNow(0, 0, false); }

// Giro no proprio eixo. v>0 = horario (para a direita)
inline void spin(int16_t v)     { set(v, -v); }
inline void spinNow(int16_t v)  { applyNow(v, -v); }

inline void forward(int16_t v)  { set(v, v); }
inline void back(int16_t v)     { set(-v, -v); }

void begin() {
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, LOW);          // nasce em standby = seguro

  pwmSetup(PIN_AIN1, CH_AIN1, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_AIN2, CH_AIN2, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_BIN1, CH_BIN1, PWM_FREQ, PWM_BITS);
  pwmSetup(PIN_BIN2, CH_BIN2, PWM_FREQ, PWM_BITS);
  applyNow(0, 0, false);
  wake(false);
}

} // namespace Mot
