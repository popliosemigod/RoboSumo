// =====================================================================
//  ROBO SUMO - brain.h
//  Maquina de estados, PID de perseguicao, confirmacao anti-ruido,
//  guarda de borda de alta prioridade e coreografia da dancinha.
// =====================================================================
#pragma once
#include "config.h"
#include "motors.h"
#include "sensors.h"
#include "sound.h"
#include "face.h"

namespace Brain {

// ---------------------------------------------------------------------
//  PERFIS DE MODO
//  P guarda a afinacao base; E e o que realmente vai para os motores.
// ---------------------------------------------------------------------
struct Eff {
  int16_t vSearch, vAttack, vMax, vReverse;
  float   kp, ki, kd;
  int16_t rangeCm, commitCm;
  uint8_t confirmHits;
  int16_t edgeBackMs, edgeTurnMs, sweepMs, rampMs;
  bool    onlyCloseAttack;      // MURALHA: so investe se estiver perto
};
Eff E;

static int16_t sc(int16_t v, float k, int16_t hi = 1000) {
  int32_t r = (int32_t)(v * k);
  return clampi(r, 0, hi);
}

void applyMode() {
  E.vSearch = P.vSearch; E.vAttack = P.vAttack; E.vMax = P.vMax; E.vReverse = P.vReverse;
  E.kp = P.kp; E.ki = P.ki; E.kd = P.kd;
  E.rangeCm = P.rangeCm; E.commitCm = 18;
  E.confirmHits = P.confirmHits;
  E.edgeBackMs = P.edgeBackMs; E.edgeTurnMs = P.edgeTurnMs;
  E.sweepMs = P.sweepMs; E.rampMs = P.rampMs;
  E.onlyCloseAttack = false;

  switch (P.mode) {
    case MODE_LESMA:                       // devagar e sempre
      E.vSearch = sc(E.vSearch, 0.42f); E.vAttack = sc(E.vAttack, 0.45f);
      E.vMax = sc(E.vMax, 0.50f);       E.vReverse = sc(E.vReverse, 0.60f);
      E.kp *= 0.60f; E.kd *= 0.70f;
      E.confirmHits += 2; E.sweepMs = (int16_t)(E.sweepMs * 1.5f);
      E.rampMs = (int16_t)(E.rampMs * 2.5f);
      E.edgeBackMs = (int16_t)(E.edgeBackMs * 1.3f);
      break;

    case MODE_CAPIROTO:                    // no limite da logica permitida
      E.vSearch = sc(E.vSearch, 1.30f); E.vAttack = 1000;
      E.vMax = 1000;                    E.vReverse = 1000;
      E.kp *= 1.45f; E.kd *= 1.35f;
      E.confirmHits = (E.confirmHits > 2) ? (uint8_t)(E.confirmHits - 1) : 2;
      E.sweepMs = (int16_t)(E.sweepMs * 0.65f);
      E.rampMs = (int16_t)(E.rampMs * 0.30f);
      E.edgeBackMs = (int16_t)(E.edgeBackMs * 0.85f);
      E.edgeTurnMs = (int16_t)(E.edgeTurnMs * 0.85f);
      E.commitCm = 26;                   // se compromete de mais longe
      break;

    case MODE_CACADOR:                     // varredura larga, alcance longo
      E.rangeCm = clampi((int32_t)(E.rangeCm * 1.35f), 20, 130);
      E.vSearch = sc(E.vSearch, 1.20f);
      E.sweepMs = (int16_t)(E.sweepMs * 0.80f);
      break;

    case MODE_MURALHA:                     // fica no centro e so contra-ataca
      E.vSearch = sc(E.vSearch, 0.35f);
      E.vAttack = sc(E.vAttack, 0.90f);
      E.sweepMs = (int16_t)(E.sweepMs * 2.0f);
      E.onlyCloseAttack = true;
      break;

    default: break;
  }
  T.mode = P.mode;
}

// ---------------------------------------------------------------------
//  ESTADO INTERNO
// ---------------------------------------------------------------------
RoboState st = ST_IDLE;
uint32_t  stSince = 0;
uint32_t  armedAt = 0;
volatile bool edgeOverride = false;

int8_t   lastBearing = 1;        // por onde o alvo sumiu (1 = direita)
uint8_t  hits = 0, misses = 0;
int16_t  prevValid = -1;
int8_t   sweepDir = 1;
uint32_t sweepFlip = 0;
uint8_t  sweepCount = 0;

float    iTerm = 0, prevErr = 0;
uint32_t prevPidMs = 0;

uint32_t attackStart = 0;
int16_t  attackLockDist = 0;
int16_t  closestSeen = 999;
uint32_t commitSince = 0;
uint32_t lastProgress = 0;

void go(RoboState s) {
  if (st == s) return;
  st = s; stSince = millis(); T.state = s;
}

// ---------------------------------------------------------------------
//  FUSAO DOS ULTRASSONICOS + CONFIANCA
// ---------------------------------------------------------------------
struct Sight { bool valid; int16_t dist; float err; };

Sight look() {
  Sight s = { false, -1, 0 };
  int16_t dl = Sens::cmL, dr = Sens::cmR;
  bool vl = (dl > 1 && dl <= E.rangeCm);
  bool vr = (dr > 1 && dr <= E.rangeCm);

  // Com um olho so nao existe diferenca entre olhos para virar: o erro
  // fica zero e o robo avanca reto. Quem encontra o alvo e a varredura.
  bool caolho = Sens::caolho();

  if (vl && vr) {
    s.dist = (dl < dr) ? dl : dr;
    s.err  = (float)(dl - dr);                    // + = alvo a direita
  } else if (vl) {
    s.dist = dl;
    s.err  = caolho ? 0.0f : -(float)E.rangeCm * 0.75f;   // so o olho esquerdo ve
  } else if (vr) {
    s.dist = dr;
    s.err  = caolho ? 0.0f : +(float)E.rangeCm * 0.75f;
  } else {
    return s;
  }
  s.valid = true;
  return s;
}

// Confirma que a leitura e um oponente de verdade e nao ruido:
//  1) precisa de N leituras validas consecutivas
//  2) o salto entre leituras nao pode ser absurdo (P.jumpCm)
void updateConfidence(const Sight& s) {
  if (s.valid) {
    bool coherent = (prevValid < 0) || (abs(s.dist - prevValid) <= (int16_t)P.jumpCm);
    if (coherent) { if (hits < 250) hits++; misses = 0; }
    else          { hits = 1; }
    prevValid = s.dist;
    lastBearing = (s.err > 0) ? 1 : -1;
  } else {
    if (misses < 250) misses++;
    if (misses >= P.loseMisses) { hits = 0; prevValid = -1; }
  }
  uint8_t need = E.confirmHits ? E.confirmHits : 1;
  uint16_t c = (uint16_t)hits * 100 / need;
  T.confidence = (uint8_t)(c > 100 ? 100 : c);
  T.distFused  = s.valid ? s.dist : -1;
  T.bearing    = s.valid ? (int8_t)((s.err > 8) ? 1 : (s.err < -8 ? -1 : 0)) : 0;
}

// ---------------------------------------------------------------------
//  PID DE DIRECIONAMENTO
// ---------------------------------------------------------------------
float pid(float err) {
  uint32_t now = millis();
  float dt = (now - prevPidMs) / 1000.0f;
  if (dt <= 0 || dt > 0.2f) dt = 0.01f;
  prevPidMs = now;

  float p = E.kp * err;
  iTerm += err * dt;
  iTerm = constrain(iTerm, -120.0f, 120.0f);       // anti-windup
  float i = E.ki * iTerm;
  float d = E.kd * (err - prevErr) / dt * 0.02f;   // derivada suavizada
  prevErr = err;

  float out = constrain(p + i + d, -1000.0f, 1000.0f);
  T.pidP = p; T.pidI = i; T.pidD = d; T.pidOut = out;
  return out;
}

inline void pidReset() { iTerm = 0; prevErr = 0; prevPidMs = millis(); }

// ---------------------------------------------------------------------
//  GUARDA DE BORDA  (task de prioridade maxima, acordada pela ISR do IR)
//  Essa e a rotina que impede o robo de cair da arena.
// ---------------------------------------------------------------------
void edgeTask(void*) {
  for (;;) {
    if (xSemaphoreTake(Sens::edgeSem, portMAX_DELAY) != pdTRUE) continue;

    if (!T.armed || st == ST_IDLE || st == ST_MANUAL || st == ST_COUNTDOWN) continue;
    if (st == ST_DANCE) continue;                 // dancinha nao aciona borda

    bool L = Sens::edgeL, R = Sens::edgeR;
    if (!L && !R) continue;

    edgeOverride = true;
    RoboState back = ST_SEARCH;
    go(ST_EDGE);
    Face::set(Face::EX_EDGE);
    Snd::play(Snd::MEL_EDGE);

    // 1) TRAVA IMEDIATA - freio eletrico, mata a inercia
    Mot::applyNow(0, 0, true);
    vTaskDelay(pdMS_TO_TICKS(18));

    // 2) RECUO reto, potencia cheia
    int16_t vr = E.vReverse;
    Mot::applyNow(-vr, -vr);
    uint32_t t0 = millis();
    while (millis() - t0 < (uint32_t)E.edgeBackMs) {
      // se a borda aparecer do outro lado durante o recuo, corta na hora
      vTaskDelay(pdMS_TO_TICKS(4));
    }

    // 3) GIRO para dentro da arena
    //    borda a esquerda -> gira para a direita, e vice-versa.
    //    borda nos dois   -> meia-volta completa.
    int16_t turn = E.vReverse;
    uint32_t turnMs = E.edgeTurnMs;
    int8_t dir;
    if (L && R) { dir = (lastBearing >= 0) ? 1 : -1; turnMs = E.edgeTurnMs * 2; }
    else if (L)  dir = +1;
    else         dir = -1;

    Mot::applyNow(turn * dir, -turn * dir);
    t0 = millis();
    while (millis() - t0 < turnMs) vTaskDelay(pdMS_TO_TICKS(4));

    Mot::applyNow(0, 0, true);
    vTaskDelay(pdMS_TO_TICKS(15));

    T.nEdgeSaves++;
    evPush(EV_EDGE, (L && R) ? 2 : (L ? -1 : 1), (int16_t)(millis() - t0));

    // volta pro combate procurando por onde o alvo estava
    hits = 0; misses = 0; prevValid = -1;
    pidReset();
    sweepDir = dir;   // continua girando no mesmo sentido da fuga
    sweepFlip = millis();
    go(back);
    Face::set(Face::EX_SEARCH);
    if (P.mode == MODE_CAPIROTO) Snd::play(Snd::MEL_GALLOP, true);
    else                          Snd::play(Snd::MEL_BATTLE, true);
    edgeOverride = false;
  }
}

// ---------------------------------------------------------------------
//  DANCINHA DA VITORIA
//  Coreografia em passos {esq, dir, ms}. Casada com o groove do buzzer.
// ---------------------------------------------------------------------
struct Step { int16_t l, r; uint16_t ms; };
const Step DANCE[] = {
  // "chamada": dois giros curtinhos pra cada lado
  { 650,-650,220}, {-650, 650,220}, { 650,-650,220}, {-650, 650,220},
  {   0,   0, 90},
  // "requebrado": vai e volta miudinho
  { 520, 520,150}, {-520,-520,150}, { 520, 520,150}, {-520,-520,150},
  {   0,   0, 90},
  // "parafuso": giro longo de 360
  { 850,-850,720},
  {   0,   0,120},
  // "sacode": tremidinha rapida
  { 700,-700, 90}, {-700, 700, 90}, { 700,-700, 90}, {-700, 700, 90},
  { 700,-700, 90}, {-700, 700, 90},
  {   0,   0,120},
  // "arrastadinho": um lado de cada vez
  { 700,   0,260}, {   0, 700,260}, { 700,   0,260}, {   0, 700,260},
  {   0,   0,100},
  // "final": parafuso ao contrario e pose
  {-900, 900,760},
  {   0,   0,150},
  { 600, 600,180}, {-600,-600,180},
  {   0,   0,400}
};
const uint8_t DANCE_N = sizeof(DANCE) / sizeof(DANCE[0]);
uint8_t  danceI = 0;
uint32_t danceStepAt = 0;

void startDance() {
  danceI = 0; danceStepAt = millis();
  Snd::play(Snd::MEL_DANCE, false);
  Face::set(Face::EX_DANCE);
  go(ST_DANCE);
}

// ---------------------------------------------------------------------
//  CONTROLE PRINCIPAL - chamado a ~200 Hz
// ---------------------------------------------------------------------
uint32_t countdownStart = 0;
uint8_t  lastBeepSec = 99;
uint32_t lastSeenMs = 0;
bool     wasCommitting = false;

void arm() {
  applyMode();
  armedAt = millis();
  T.armed = true;
  T.nAttacks = T.nEdgeSaves = T.nLost = 0;
  T.timeAttackingMs = 0;
  EVN = 0; EVI = 0;
  evPush(EV_ARM, P.mode, 0);
  hits = misses = 0; prevValid = -1; pidReset();
  countdownStart = millis(); lastBeepSec = 99;
  Mot::wake(true);
  if (P.mode == MODE_DANCA) { startDance(); return; }
  go(ST_COUNTDOWN);
  Face::set(Face::EX_COUNT);
}

void disarm() {
  T.armed = false;
  Mot::applyNow(0, 0, true);
  delay(30);
  Mot::coast();
  Mot::wake(false);
  Snd::stop();
  go(ST_IDLE);
  Face::set(Face::EX_IDLE);
  T.confidence = 0;
}

uint32_t lastManualMs = 0;

void manual(int16_t l, int16_t r) {
  go(ST_MANUAL);
  T.armed = true;
  lastManualMs = millis();
  Mot::wake(true);
  Mot::applyNow(l, r);
  Face::set(Face::EX_IDLE);
  Face::look(l > r ? 1 : (r > l ? -1 : 0));
}

void tick() {
  if (edgeOverride) return;                       // o guarda de borda manda

  uint32_t now = millis();
  T.uptimeMs = now;

  // ---- protecoes ----
  if (Mot::fault()) {
    T.drvFault = true;
    if (st != ST_IDLE && st != ST_FAULT) {
      evPush(EV_FAULT, 0, 0); Face::say("DRV8833 FAULT");
      Face::set(Face::EX_FAULT); go(ST_FAULT); Mot::applyNow(0,0,false);
    }
  } else T.drvFault = false;

  // O corte por bateria baixa so passa a valer depois de ver UMA vez uma
  // tensao plausivel de bateria. Sem isso, o divisor desligado (pino
  // flutuando) travaria o robo em FALHA no meio do teste, sem motivo.

  Sight s = look();
  updateConfidence(s);
  if (s.valid) lastSeenMs = now;

  switch (st) {

    // -----------------------------------------------------------------
    case ST_IDLE:
      Mot::set(0, 0);
      Face::set(now - stSince > 20000 ? Face::EX_SLEEP : Face::EX_IDLE);
      break;

    // -----------------------------------------------------------------
    case ST_COUNTDOWN: {
      Mot::applyNow(0, 0, true);
      uint32_t el = now - countdownStart;
      T.countdownLeft = (el >= P.countdownMs) ? 0 : (P.countdownMs - el);
      uint8_t sec = (uint8_t)(T.countdownLeft / 1000);
      if (sec != lastBeepSec) {
        lastBeepSec = sec;
        if (T.countdownLeft > 0) Snd::beep(1400, 70), Snd::beep(0, 120);
      }
      Face::set(Face::EX_COUNT);
      if (el >= P.countdownMs) {
        Snd::beep(2400, 400); Snd::beep(0, 60);
        pidReset(); hits = misses = 0;
        sweepDir = 1; sweepFlip = now; sweepCount = 0;
        go(ST_SEARCH);
        Face::set(P.mode == MODE_CAPIROTO ? Face::EX_EVIL : Face::EX_SEARCH);
        if (P.mode == MODE_CAPIROTO) Snd::play(Snd::MEL_GALLOP, true);
        else if (P.mode == MODE_LESMA) Snd::play(Snd::MEL_TAUNT, true);
        else Snd::play(Snd::MEL_BATTLE, true);
      }
    } break;

    // -----------------------------------------------------------------
    case ST_SEARCH: {
      // varredura girando no proprio eixo, invertendo o sentido a cada
      // sweepMs. A cada 3 varreduras da uma "reposicionada" curta para
      // mudar o circulo de busca e nao ficar preso num canto.
      if (now - sweepFlip > (uint32_t)E.sweepMs) {
        sweepFlip = now; sweepDir = -sweepDir; sweepCount++;
      }
      bool repos = (sweepCount % 3 == 2) && ((now - sweepFlip) < 260);
      if (repos) Mot::set(E.vSearch, E.vSearch);
      else       Mot::spin(E.vSearch * sweepDir);

      Face::set(P.mode == MODE_CAPIROTO ? Face::EX_EVIL : Face::EX_SEARCH);
      Face::look(sweepDir);

      if (s.valid) {
        go(ST_LOCK);
        hits = 1; prevValid = s.dist;
      }
    } break;

    // -----------------------------------------------------------------
    case ST_LOCK: {
      // aproximacao cautelosa enquanto confirma que o alvo e real
      float out = pid(s.valid ? s.err : prevErr);
      int16_t base = E.vSearch;
      Mot::set(clampi(base + (int32_t)out, -1000, 1000),
               clampi(base - (int32_t)out, -1000, 1000));
      Face::set(Face::EX_LOCK);
      Face::look(T.bearing);

      if (hits >= E.confirmHits && s.valid) {
        if (E.onlyCloseAttack && s.dist > 40) break;   // MURALHA espera chegar perto
        attackStart = now; attackLockDist = s.dist;
        closestSeen = s.dist; lastProgress = now;
        commitSince = 0; wasCommitting = false;
        T.nAttacks++;
        evPush(EV_LOCK, s.dist, T.confidence);
        Snd::play(Snd::MEL_LOCK);
        go(ST_ATTACK);
        Face::set(P.mode == MODE_CAPIROTO ? Face::EX_EVIL : Face::EX_ATTACK);
      }
      if (misses >= P.loseMisses) { go(ST_SEARCH); sweepFlip = now; sweepDir = lastBearing; }
    } break;

    // -----------------------------------------------------------------
    case ST_ATTACK: {
      T.timeAttackingMs += 5;

      float err = s.valid ? s.err : (prevErr * 0.85f);
      float out = pid(err);

      int16_t base = E.vAttack;
      bool commit = s.valid && s.dist <= E.commitCm;

      if (commit) {
        // ja esta em cima do oponente: reduz a correcao e vai com tudo.
        // Empurrar reto vale mais que corrigir angulo aqui.
        out *= 0.30f;
        base = E.vMax;
        if (!commitSince) commitSince = now;
        wasCommitting = true;
      } else {
        commitSince = 0;
        // quanto maior o erro, mais o robo prioriza girar do que avancar
        float m = fabsf(out) / 1000.0f; if (m > 1.0f) m = 1.0f;
        base = (int16_t)(base * (1.0f - m * 0.55f));
      }

      Mot::set(clampi(base + (int32_t)out, -1000, 1000),
               clampi(base - (int32_t)out, -1000, 1000));
      Face::set(P.mode == MODE_CAPIROTO ? Face::EX_EVIL : Face::EX_ATTACK);
      Face::look(T.bearing);

      if (s.valid && s.dist < closestSeen - 2) { closestSeen = s.dist; lastProgress = now; }

      // ---- vitoria provavel: empurrou colado e o alvo sumiu ----
      if (wasCommitting && !s.valid && (now - lastSeenMs) > 450 &&
          commitSince == 0 && (now - attackStart) > 700) {
        evPush(EV_WIN, attackLockDist, (int16_t)(now - attackStart));
        Mot::applyNow(0, 0, true);
        if (P.autoRestart) { startDance(); }
        else { Snd::play(Snd::MEL_CHARGE); Face::set(Face::EX_WIN); go(ST_SEARCH); }
        break;
      }

      // ---- alvo perdido ----
      if (misses >= P.loseMisses) {
        T.nLost++;
        evPush(EV_LOST, closestSeen, (int16_t)(now - attackStart));
        go(ST_SEARCH); sweepFlip = now; sweepDir = lastBearing;
        Face::set(Face::EX_SEARCH);
        break;
      }

      // ---- empurrando sem sair do lugar: tenta outro angulo ----
      if (!commit && (now - lastProgress) > (uint32_t)P.stuckMs) {
        evPush(EV_STUCK, s.dist, 0);
        go(ST_UNSTUCK);
        Face::set(Face::EX_DIZZY);
      }
    } break;

    // -----------------------------------------------------------------
    case ST_UNSTUCK: {
      // recua, angula e volta pra briga por outra linha
      uint32_t el = now - stSince;
      if      (el < 200) Mot::applyNow(-E.vReverse, -E.vReverse);
      else if (el < 420) Mot::applyNow(E.vSearch * lastBearing, -E.vSearch * lastBearing);
      else {
        pidReset(); hits = misses = 0; lastProgress = now;
        go(ST_SEARCH); sweepFlip = now;
      }
    } break;

    // -----------------------------------------------------------------
    case ST_DANCE: {
      if (danceI >= DANCE_N) {
        Mot::applyNow(0, 0, true);
        Snd::play(Snd::MEL_CHARGE);
        Face::set(Face::EX_WIN);
        if (P.autoRestart && P.mode != MODE_DANCA) {
          hits = misses = 0; pidReset();
          sweepFlip = now; go(ST_SEARCH);
        } else { T.armed = false; go(ST_IDLE); Mot::wake(false); }
        break;
      }
      const Step& sp = DANCE[danceI];
      if (now - danceStepAt >= sp.ms) { danceI++; danceStepAt = now; }
      else Mot::applyNow(sp.l, sp.r);
      Face::set(Face::EX_DANCE);
    } break;

    // -----------------------------------------------------------------
    case ST_MANUAL:
      // Comandado direto por manual(). Se o painel calar (aba fechada, Wi-Fi
      // caiu), o robo para sozinho em 700 ms em vez de sair andando.
      if (now - lastManualMs > 700 && (T.pwmL || T.pwmR)) Mot::applyNow(0, 0, true);
      break;

    case ST_FAULT:
      Mot::applyNow(0, 0, false);
      Mot::wake(false);
      break;
  }

  if (st != ST_MANUAL && st != ST_DANCE && st != ST_UNSTUCK) Mot::update();
}

void begin() {
  applyMode();
  go(ST_IDLE);
  Face::set(Face::EX_IDLE);
}

} // namespace Brain
