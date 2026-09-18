// =====================================================================
//  ROBO SUMO v2 - sensors.h
//  Olho: 1x HC-SR04 (ultrassonico, 40 kHz), lido por interrupcao.
//  Borda: 1x modulo IR digital com ISR + ressincronizacao periodica.
// =====================================================================
#pragma once
#include "config.h"

namespace Sens {

// ---------------------------------------------------------------------
//  OLHO - HC-SR04
//
//  O modulo responde a um pulso de 10 us no TRIG com oito ciclos a
//  40 kHz, e mantem ECHO em ALTO pelo tempo de ida e volta do som. A
//  conta esta na eq. 3 da nota de aplicacao: cm = us / 58.
//
//  A largura do ECHO e medida por INTERRUPCAO nas duas bordas, com
//  micros(). A alternativa obvia - pulseIn() - foi descartada de
//  proposito: ela e espera ocupada e segura a CPU por ate 38 ms por
//  leitura. Numa placa de nucleo unico isso disputaria tempo com a
//  guarda de borda, que e justamente a rotina que nao pode atrasar.
// ---------------------------------------------------------------------
volatile uint32_t echoIni  = 0;      // micros() da subida do ECHO
volatile uint32_t echoLarg = 0;      // largura medida, em us
volatile bool     echoNovo = false;  // ha medida fresca esperando

int16_t cm = -1;                     // distancia publicada (-1 = nada valido)

void IRAM_ATTR isrEcho() {
  if (digitalRead(PIN_US_ECHO)) {
    echoIni = micros();
  } else if (echoIni) {
    echoLarg = micros() - echoIni;
    echoIni  = 0;
    echoNovo = true;
  }
}

// Uma leitura a cada US_PERIODO_MS. O intervalo nao e folga: eco de uma
// medida ainda viajando pela sala chega durante a medida seguinte e vira
// um alvo perto que nao existe. Dar tempo para o som morrer e mais
// barato que filtrar depois.
void usTask(void*) {
  for (;;) {
    echoNovo = false;
    echoIni  = 0;

    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    // Espera o eco cedendo a CPU, em vez de girar em cima do pino.
    uint32_t t0 = millis();
    while (!echoNovo && (millis() - t0) < 30) vTaskDelay(pdMS_TO_TICKS(2));

    bool ok = false;
    if (echoNovo) {
      uint32_t us = echoLarg;
      T.echoUs = (uint16_t)(us > 65535UL ? 65535UL : us);
      int16_t c = (int16_t)(us / 58UL);
      // Fora de alcance util nao e defeito: o dojo tem 77 cm, entao eco
      // de 3 m e parede, e parede nao se ataca.
      ok = (us > 0 && us < US_TIMEOUT_US && c > 1 && c <= (int16_t)P.rangeCm);
      cm = ok ? c : -1;
    } else {
      T.echoUs = 0;       // nenhum eco voltou: nada na frente, ou modulo mudo
      cm = -1;
    }

    if (ok) T.usFail = 0;
    else if (T.usFail < 60000) T.usFail++;
    T.dist = cm;

    vTaskDelay(pdMS_TO_TICKS(US_PERIODO_MS));
  }
}

// ---------------------------------------------------------------------
//  BORDA - modulo IR digital
//
//  A interrupcao e o caminho rapido, mas ela so dispara em TRANSICAO: se
//  o robo for ligado ja em cima da faixa branca, transicao nenhuma
//  acontece e o pino fica mentindo em silencio. Por isso o pino tambem e
//  relido a cada ciclo de controle, em pollIr().
// ---------------------------------------------------------------------
SemaphoreHandle_t edgeSem = nullptr;
volatile bool     edge = false;

static inline bool ehBorda(bool nivel) {
  return P.irActiveLow ? (nivel == LOW) : (nivel == HIGH);
}

void IRAM_ATTR isrIr() {
  bool v = ehBorda(digitalRead(PIN_IR_BORDA));
  edge = v;
  if (v) {
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(edgeSem, &hp);
    if (hp) portYIELD_FROM_ISR();
  }
}

void pollIr() {
  bool d = digitalRead(PIN_IR_BORDA);
  bool v = ehBorda(d);
  T.irDo = d;
  if (v && !edge) xSemaphoreGive(edgeSem);
  edge = v;
  T.irBorda = v;
}

// ---------------------------------------------------------------------
//  AUTOTESTE
//
//  Vale a leitura antes de suspeitar da logica: a maior parte dos
//  problemas deste projeto sempre foi fiacao.
// ---------------------------------------------------------------------
void selfTest() {
  Serial.println("[teste] --- autoteste ---");
  Serial.printf("[teste] chip=%s rev=%u nucleos=%u flash=%uMB\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));

  bool ir = digitalRead(PIN_IR_BORDA);
  Serial.printf("[teste] IR de borda (GPIO %u): pino %s -> %s\n",
                PIN_IR_BORDA, ir ? "ALTO" : "BAIXO",
                ehBorda(ir) ? "VENDO BORDA" : "superficie segura");
  if (!ir && P.irActiveLow)
    Serial.println("[teste]   atencao: BAIXO em repouso tambem e o que um fio solto "
                   "faria se o pino nao tivesse pull-up. Confira o jumper do OUT.");

  // O ultrassonico so prova que existe respondendo. Um disparo direto,
  // sem depender da task, separa "modulo mudo" de "nada na frente".
  echoNovo = false; echoIni = 0;
  digitalWrite(PIN_US_TRIG, HIGH); delayMicroseconds(10); digitalWrite(PIN_US_TRIG, LOW);
  uint32_t t0 = millis();
  while (!echoNovo && (millis() - t0) < 40) delay(1);
  if (echoNovo)
    Serial.printf("[teste] HC-SR04: eco de %lu us -> %ld cm\n",
                  (unsigned long)echoLarg, (long)(echoLarg / 58UL));
  else
    Serial.println("[teste] HC-SR04: NENHUM eco. Se nao ha nada a 4 m isso e normal; "
                   "se ha, confira TRIG(6), ECHO(7), 3V3 e o GND comum.");

  Serial.printf("[teste] receptor do juiz (GPIO %u) = %s (repouso e ALTO)\n",
                PIN_IR_JUIZ, digitalRead(PIN_IR_JUIZ) ? "ALTO" : "BAIXO");
  Serial.printf("[teste] nFAULT das pontes (GPIO %u) = %s\n",
                PIN_DRV_FAULT, digitalRead(PIN_DRV_FAULT) ? "ok" : "EM FALHA");
  Serial.println("[teste] --- fim ---");
}

// ---------------------------------------------------------------------
void begin() {
  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_US_ECHO), isrEcho, CHANGE);

  // Pull-up interno: fio solto repousa em ALTO ("seguro") em vez de
  // BAIXO ("borda"), e deixa de se disfarcar de leitura boa.
  pinMode(PIN_IR_BORDA, INPUT_PULLUP);
  edgeSem = xSemaphoreCreateBinary();
  attachInterrupt(digitalPinToInterrupt(PIN_IR_BORDA), isrIr, CHANGE);
}

} // namespace Sens
