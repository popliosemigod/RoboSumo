// =====================================================================
//  ROBO SUMO v4 - brain.h
//  Maquina de estados, confirmacao anti-ruido, guarda de borda de alta
//  prioridade e coreografia da dancinha.
//
//  A ORDEM DE PRIORIDADE DO COMPORTAMENTO, do mais forte para o mais
//  fraco, e a razao de ser deste arquivo:
//
//    1. BORDA. O IR nao pode falhar. Viu a faixa clara, interrompe tudo
//       e volta para dentro. Nenhuma perseguicao tem precedencia.
//    2. ALVO. Enquanto o HC-SR04 ve alguem a frente e o IR nao ve borda,
//       avanca em cima e empurra.
//    3. BUSCA. Sem alvo e sem borda, gira procurando.
//
//  A prioridade 1 nao e uma checagem dentro do laco de controle - e uma
//  task propria, de prioridade maior, que preempta o laco.
// =====================================================================
#pragma once
#include "config.h"
#include "motors.h"
#include "sensors.h"

namespace Brain {

// ---------------------------------------------------------------------
//  PERFIS DE MODO
//  P guarda a afinacao base; E e o que realmente vai para os motores.
// ---------------------------------------------------------------------
struct Eff {
  int16_t vSearch, vAttack, vMax, vReverse;
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
  E.rangeCm = P.rangeCm; E.commitCm = 18;
  E.confirmHits = P.confirmHits;
  E.edgeBackMs = P.edgeBackMs; E.edgeTurnMs = P.edgeTurnMs;
  E.sweepMs = P.sweepMs; E.rampMs = P.rampMs;
  E.onlyCloseAttack = false;

  switch (P.mode) {
    case MODE_LESMA:                       // devagar e sempre
      E.vSearch = sc(E.vSearch, 0.42f); E.vAttack = sc(E.vAttack, 0.45f);
      E.vMax = sc(E.vMax, 0.50f);       E.vReverse = sc(E.vReverse, 0.60f);
      E.confirmHits += 2; E.sweepMs = (int16_t)(E.sweepMs * 1.5f);
      E.rampMs = (int16_t)(E.rampMs * 2.5f);
      E.edgeBackMs = (int16_t)(E.edgeBackMs * 1.3f);
      break;

    case MODE_CAPIROTO:                    // no limite da logica permitida
      E.vSearch = sc(E.vSearch, 1.30f); E.vAttack = 1000;
      E.vMax = 1000;                    E.vReverse = 1000;
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

uint8_t  hits = 0, misses = 0;
int16_t  prevValid = -1;
int8_t   sweepDir = 1;
uint32_t sweepFlip = 0;
uint8_t  sweepCount = 0;

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
//  VISAO + CONFIANCA
//
//  Um olho so responde "ha algo a N cm na minha frente", e nada mais. A
//  direcao nao vem do sensor: vem de para onde o robo estava girando
//  quando o alvo apareceu.
// ---------------------------------------------------------------------
struct Sight { bool valid; int16_t dist; };

Sight look() {
  int16_t d = Sens::cm;
  Sight s = { (d > 1 && d <= E.rangeCm), d };
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
  } else {
    if (misses < 250) misses++;
    if (misses >= P.loseMisses) { hits = 0; prevValid = -1; }
  }
  uint8_t need = E.confirmHits ? E.confirmHits : 1;
  uint16_t c = (uint16_t)hits * 100 / need;
  T.confidence = (uint8_t)(c > 100 ? 100 : c);
}

// ---------------------------------------------------------------------
//  GUARDA DE BORDA  -  PRIORIDADE MAXIMA
//
//  Esta task varre o AO do IR a 1 kHz e nao faz mais nada. E a trava de
//  seguranca que impede o robo de sair da arena, e por isso ela roda
//  acima do laco de controle: quando a borda aparece, ela preempta o
//  ataque no meio, sem esperar o laco terminar o ciclo.
//
//  A varredura substituiu a interrupcao da v2 por um motivo simples:
//  interrupcao de GPIO dispara em nivel logico, e o que temos agora e
//  uma tensao no ADC. 1 ms de latencia no pior caso - a 1 m/s, um
//  milimetro de avanco.
//
//  Com UM sensor, montado na frente, nao ha "lado que nao viu": quando
//  o IR acusa, a borda esta sob o nariz. A fuga e sempre recuar e girar,
//  e o sentido do giro alterna a cada salvamento - insistir sempre no
//  mesmo lado faz o robo colher a mesma borda de novo quando ele esta
//  preso entre duas.
// ---------------------------------------------------------------------
int8_t giroFuga = 1;

void edgeTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    bool viuBorda = Sens::lerBorda();     // atualiza T.irMv sempre, armado ou nao

    bool podeAgir = T.armed &&
                    st != ST_IDLE && st != ST_MANUAL &&
                    st != ST_COUNTDOWN && st != ST_DANCE;

    if (viuBorda && podeAgir) {
      edgeOverride = true;
      go(ST_EDGE);

      // 1) TRAVA IMEDIATA - freio eletrico, mata a inercia
      Mot::applyNow(0, 0, true);
      vTaskDelay(pdMS_TO_TICKS(18));

      // 2) RECUO reto, potencia cheia
      int16_t vr = E.vReverse;
      Mot::applyNow(-vr, -vr);
      uint32_t t0 = millis();
      while (millis() - t0 < (uint32_t)E.edgeBackMs) vTaskDelay(pdMS_TO_TICKS(4));

      // 3) GIRO para dentro da arena
      int16_t turn = E.vReverse;
      int8_t  dir  = giroFuga;
      giroFuga = (int8_t)-giroFuga;           // da proxima vez, para o outro lado

      Mot::applyNow(turn * dir, -turn * dir);
      t0 = millis();
      while (millis() - t0 < (uint32_t)E.edgeTurnMs) vTaskDelay(pdMS_TO_TICKS(4));

      Mot::applyNow(0, 0, true);
      vTaskDelay(pdMS_TO_TICKS(15));

      T.nEdgeSaves++;

      // volta pro combate girando no mesmo sentido da fuga
      hits = 0; misses = 0; prevValid = -1;
      sweepDir = dir;
      sweepFlip = millis();
      go(ST_SEARCH);
      edgeOverride = false;
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(1));   // 1 kHz
  }
}

// ---------------------------------------------------------------------
//  DANCINHA DA VITORIA
//  Coreografia em passos {esq, dir, ms}.
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
  go(ST_DANCE);
}

// ---------------------------------------------------------------------
//  CONTROLE PRINCIPAL - chamado a ~200 Hz
// ---------------------------------------------------------------------
uint32_t countdownStart = 0;
uint32_t lastSeenMs = 0;
bool     wasCommitting = false;

void arm() {
  applyMode();
  armedAt = millis();
  T.armed = true;
  T.nAttacks = T.nEdgeSaves = T.nLost = 0;
  T.timeAttackingMs = 0;
  hits = misses = 0; prevValid = -1;
  countdownStart = millis();
  Mot::wake(true);
  if (P.mode == MODE_DANCA) { startDance(); return; }
  go(ST_COUNTDOWN);
}

void disarm() {
  T.armed = false;
  Mot::applyNow(0, 0, true);
  delay(30);
  Mot::coast();
  Mot::wake(false);
  go(ST_IDLE);
  T.confidence = 0;
}

uint32_t lastManualMs = 0;

void manual(int16_t l, int16_t r) {
  go(ST_MANUAL);
  T.armed = true;
  lastManualMs = millis();
  Mot::wake(true);
  Mot::applyNow(l, r);
}

void tick() {
  if (edgeOverride) return;                       // o guarda de borda manda

  uint32_t now = millis();
  T.uptimeMs = now;

  Sight s = look();
  updateConfidence(s);
  if (s.valid) lastSeenMs = now;

  switch (st) {

    // -----------------------------------------------------------------
    case ST_IDLE:
      Mot::set(0, 0);
      break;

    // -----------------------------------------------------------------
    case ST_COUNTDOWN: {
      Mot::applyNow(0, 0, true);
      uint32_t el = now - countdownStart;
      T.countdownLeft = (el >= P.countdownMs) ? 0 : (P.countdownMs - el);
      if (el >= P.countdownMs) {
        hits = misses = 0;
        sweepDir = 1; sweepFlip = now; sweepCount = 0;
        go(ST_SEARCH);
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

      if (s.valid) {
        go(ST_LOCK);
        hits = 1; prevValid = s.dist;
      }
    } break;

    // -----------------------------------------------------------------
    case ST_LOCK: {
      // Aproximacao cautelosa enquanto confirma que o alvo e real. Sem
      // erro lateral para corrigir, "cautelosa" e simplesmente devagar e
      // reto: o alvo ja esta na frente, foi assim que ele foi visto.
      Mot::set(E.vSearch, E.vSearch);

      if (hits >= E.confirmHits && s.valid) {
        if (E.onlyCloseAttack && s.dist > 40) break;   // MURALHA espera chegar perto
        attackStart = now; attackLockDist = s.dist;
        closestSeen = s.dist; lastProgress = now;
        commitSince = 0; wasCommitting = false;
        T.nAttacks++;
        go(ST_ATTACK);
      }
      if (misses >= P.loseMisses) { go(ST_SEARCH); sweepFlip = now; }
    } break;

    // -----------------------------------------------------------------
    case ST_ATTACK: {
      T.timeAttackingMs += 5;

      int16_t base = E.vAttack;
      bool commit = s.valid && s.dist <= E.commitCm;

      if (commit) {
        // ja esta em cima do oponente: vai com tudo e empurra reto
        base = E.vMax;
        if (!commitSince) commitSince = now;
        wasCommitting = true;
      } else {
        commitSince = 0;
      }

      Mot::set(base, base);

      if (s.valid && s.dist < closestSeen - 2) { closestSeen = s.dist; lastProgress = now; }

      // ---- vitoria provavel: empurrou colado e o alvo sumiu ----
      if (wasCommitting && !s.valid && (now - lastSeenMs) > 450 &&
          commitSince == 0 && (now - attackStart) > 700) {
        Mot::applyNow(0, 0, true);
        if (P.autoRestart) { startDance(); }
        else { go(ST_SEARCH); }
        break;
      }

      // ---- alvo perdido ----
      if (misses >= P.loseMisses) {
        T.nLost++;
        go(ST_SEARCH); sweepFlip = now;
        break;
      }

      // ---- empurrando sem sair do lugar: tenta outro angulo ----
      if (!commit && (now - lastProgress) > (uint32_t)P.stuckMs) {
        go(ST_UNSTUCK);
      }
    } break;

    // -----------------------------------------------------------------
    case ST_UNSTUCK: {
      // recua, angula e volta pra briga por outra linha
      uint32_t el = now - stSince;
      if      (el < 200) Mot::applyNow(-E.vReverse, -E.vReverse);
      else if (el < 420) Mot::applyNow(E.vSearch * sweepDir, -E.vSearch * sweepDir);
      else {
        hits = misses = 0; lastProgress = now;
        go(ST_SEARCH); sweepFlip = now;
      }
    } break;

    // -----------------------------------------------------------------
    case ST_DANCE: {
      if (danceI >= DANCE_N) {
        Mot::applyNow(0, 0, true);
        if (P.autoRestart && P.mode != MODE_DANCA) {
          hits = misses = 0;
          sweepFlip = now; go(ST_SEARCH);
        } else { T.armed = false; go(ST_IDLE); Mot::wake(false); }
        break;
      }
      const Step& sp = DANCE[danceI];
      if (now - danceStepAt >= sp.ms) { danceI++; danceStepAt = now; }
      else Mot::applyNow(sp.l, sp.r);
    } break;

    // -----------------------------------------------------------------
    case ST_MANUAL:
      // Comandado direto por manual(). O painel reenvia a direcao a cada
      // 300 ms enquanto o botao esta ativo; se ele calar (aba fechada,
      // Wi-Fi caiu), o robo para sozinho em 700 ms em vez de sair andando.
      if (now - lastManualMs > 700 && (T.pwmL || T.pwmR)) Mot::applyNow(0, 0, true);
      break;

    // -----------------------------------------------------------------
    case ST_EDGE:
      // Quem dirige durante a fuga e edgeTask(), e enquanto ela dirige o
      // edgeOverride faz esta funcao retornar la em cima. Chegar aqui
      // significa que o estado ficou para tras depois da manobra - deixar
      // parado e mais honesto do que deixar o caso implicito.
      Mot::set(0, 0);
      break;
  }

  if (st != ST_MANUAL && st != ST_DANCE && st != ST_UNSTUCK) Mot::update();
}

void begin() {
  applyMode();
  go(ST_IDLE);
}

} // namespace Brain
