// =====================================================================
//  ROBO SUMO v4 - sensors.h
//  Olho: 1x HC-SR04 (ultrassonico, 40 kHz), lido por interrupcao.
//  Borda: 1x modulo IR pela SAIDA ANALOGICA (AO), lido no ADC1.
// =====================================================================
#pragma once
#include "config.h"

namespace Sens {

// ---------------------------------------------------------------------
//  OLHO - HC-SR04
//
//  Pulso de 10 us no TRIG; o modulo responde com oito ciclos a 40 kHz e
//  mantem ECHO em ALTO pelo tempo de ida e volta do som. cm = us / 58.
//
//  A largura do ECHO e medida por INTERRUPCAO nas duas bordas, com
//  micros(). A alternativa obvia - pulseIn() - foi descartada de
//  proposito: ela e espera ocupada e segura a CPU por ate 38 ms por
//  leitura, numa placa que so tem um nucleo.
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
//  BORDA - IR pela saida analogica
//
//  Nao existe interrupcao aqui, e isso e consequencia direta de ler o
//  AO: interrupcao de GPIO dispara em nivel logico, e o que temos e uma
//  tensao. Quem vigia a borda e uma varredura a 1 kHz na task de
//  prioridade maxima - 1 ms de latencia no pior caso, que a 1 m/s da
//  um milimetro de avanco.
//
//  Ler o AO em vez do DO e uma escolha, nao uma limitacao: o DO ja vem
//  comparado contra o trimpot do modulo, e um limiar que mora num
//  parafuso nao da para versionar, conferir em codigo nem ajustar por
//  conversa. O AO devolve o numero e o limiar vira IR_LIMIAR_MV.
// ---------------------------------------------------------------------
volatile bool borda = false;

// Le o AO e decide. Devolve true quando esta vendo a faixa clara.
bool lerBorda() {
  uint16_t mv = analogReadMilliVolts(PIN_IR_AO);
  T.irMv = mv;
#if IR_BORDA_ABAIXO
  bool v = (mv < IR_LIMIAR_MV);
#else
  bool v = (mv > IR_LIMIAR_MV);
#endif
  borda = v;
  T.irBorda = v;
  return v;
}

// ---------------------------------------------------------------------
//  CALIBRACAO DO LIMIAR
//
//  Mede por ~600 ms e devolve tambem a excursao. A media sozinha nao
//  basta: ela nao distingue "sensor parado sobre a superficie" de "robo
//  sendo movido durante a captura", e calibracao feita em movimento e
//  pior que nenhuma - ela parece boa.
// ---------------------------------------------------------------------
void medeSuperficie(const char* qual) {
  uint32_t acc = 0;
  uint16_t mn = 4095, mx = 0;
  const uint8_t N = 60;
  for (uint8_t i = 0; i < N; i++) {
    uint16_t v = analogReadMilliVolts(PIN_IR_AO);
    acc += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    delay(10);
  }
  uint16_t media = (uint16_t)(acc / N);
  Serial.printf("[ircal] %s: media %u mV (min %u, max %u, excursao %u)\n",
                qual, media, mn, mx, (unsigned)(mx - mn));
  if (mx - mn > 200)
    Serial.println("[ircal]   LEITURA INSTAVEL. Segure o robo parado sobre a "
                   "superficie e meca de novo.");
  Serial.printf("[ircal]   limiar atual: %u mV. Meca o preto e o branco e "
                "ponha IR_LIMIAR_MV no meio dos dois.\n", (unsigned)IR_LIMIAR_MV);
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

  uint16_t mv = analogReadMilliVolts(PIN_IR_AO);
  Serial.printf("[teste] IR AO (GPIO %u, ADC1): %u mV | limiar %u mV -> %s\n",
                PIN_IR_AO, mv, (unsigned)IR_LIMIAR_MV,
                lerBorda() ? "VENDO BORDA" : "superficie segura");
  if (mv < 40)
    Serial.println("[teste]   quase zero: confira o AO, o 3V3 do modulo e o "
                   "divisor. Fio solto no ADC tambem le perto de zero.");

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
                   "se ha, confira TRIG(3), ECHO(4), o divisor 1k/2k e o GND comum.");

  Serial.printf("[teste] L298N ENA/ENB (GPIO %u) = %s\n",
                PIN_EN, digitalRead(PIN_EN) ? "habilitado" : "saidas soltas");
  Serial.printf("[teste] receptor do edital: GPIO %u reservado, ainda sem leitura\n",
                PIN_RX_EDITAL);
  Serial.println("[teste] --- fim ---");
}

// ---------------------------------------------------------------------
void begin() {
  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_US_ECHO), isrEcho, CHANGE);

  // 12 bits e atenuacao cheia: a faixa util vai a ~3,1 V, e o divisor
  // 10k/10k mantem o sinal do sensor abaixo disso.
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_IR_AO, ADC_11db);

  // O pino do receptor do edital fica reservado e em repouso ALTO, para
  // nao ficar flutuando enquanto a decodificacao nao existe.
  pinMode(PIN_RX_EDITAL, INPUT_PULLUP);
}

} // namespace Sens
