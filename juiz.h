// =====================================================================
//  JUIZ - receptor infravermelho do controle do arbitro
//
//  O Artigo 18 do regulamento do Sumo Tech Fight torna este componente
//  OBRIGATORIO: todo robo deve ter um receptor de 950 nm sintonizado em
//  38 kHz para receber os comandos do juiz. Sem ele o robo nao entra na
//  arena, por mais que lute bem.
//
//  O Artigo 33 define tres estados e o controle padrao da RoboCore:
//
//      Tecla A (command 0x0C) -> READY  : pronto e IMOVEL
//      Tecla B (command 0x18) -> START  : comeca a luta
//      Tecla C (command 0x5E) -> STOP   : para de forma DEFINITIVA
//
//  A parte que mais pesa no codigo e o STOP, e ela e explicita no
//  regulamento: "ao receber o sinal de Stop, seu robo nao deve poder
//  fazer mais nada antes de ser reiniciado: ele nao pode voltar ao estado
//  Ready nem receber nenhum sinal de Start". Nao e uma pausa - e uma
//  trava que so o corte de energia desfaz. Por isso `travado` nao tem
//  caminho de volta em lugar nenhum deste arquivo.
//
//  READY tambem serve de pausa do juiz: de START da para voltar a READY.
//
//  Sobre os codigos: o Anexo A.2 da o mesmo botao em tres formas. O
//  campo "Command" (0x0C) e a leitura LSB-first do byte de comando NEC;
//  os dois valores brutos sao o mesmo quadro em ordens de bit opostas.
//  Comparamos pelo quadro bruto de 32 bits, que nao depende de qual
//  convencao a biblioteca usa, e ainda conferimos o byte de comando.
// =====================================================================
#pragma once
#include <Arduino.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "config.h"

namespace Juiz {

// Quadros brutos do Anexo A.2, nas duas representacoes da tabela.
#define JZ_A_NEW  0xF30CFF00UL   // Ready  (IRremote >= 3.0)
#define JZ_A_OLD  0x00FF30CFUL   // Ready  (IRremote 2.x)
#define JZ_B_NEW  0xE718FF00UL   // Start
#define JZ_B_OLD  0x00FF18E7UL
#define JZ_C_NEW  0xA15EFF00UL   // Stop
#define JZ_C_OLD  0x00FF7A85UL

enum Estado : uint8_t { JZ_ESPERA = 0, JZ_READY, JZ_START, JZ_STOP };

Estado   estado  = JZ_ESPERA;
bool     travado = false;          // STOP recebido: so o boot limpa
uint32_t ultimoQuadro = 0;
uint32_t ultimoMs = 0;

IRrecv receptor(PIN_IR_JUIZ, 1024, 15, true);
decode_results res;

void begin() {
  pinMode(PIN_IR_JUIZ, INPUT_PULLUP);
  receptor.enableIRIn();
  Serial.printf("[juiz] receptor de 38 kHz no GPIO %u - A=Ready B=Start C=Stop\n",
                (unsigned)PIN_IR_JUIZ);
}

// O byte de comando do quadro NEC, ja na convencao da tabela do anexo
// (LSB-first). Serve so para o log: a decisao usa o quadro inteiro.
static uint8_t comandoDe(uint32_t bruto) {
  uint8_t b = (uint8_t)((bruto >> 8) & 0xFF);
  uint8_t r = 0;
  for (uint8_t i = 0; i < 8; i++) if (b & (1 << i)) r |= (uint8_t)(0x80 >> i);
  return r;
}

// Devolve true quando um comando novo do juiz foi aceito.
bool poll() {
  if (!receptor.decode(&res)) return false;
  uint32_t q = (uint32_t)res.value;
  receptor.resume();

  // Repeticao do NEC (0xFFFFFFFF) e tecla segurada: o quadro anterior vale.
  if (q == 0xFFFFFFFFUL) q = ultimoQuadro;
  if (!q) return false;

  // O controle repete o quadro enquanto a tecla fica pressionada. Sem esta
  // janela, um toque do juiz viraria varias transicoes de estado.
  uint32_t agora = millis();
  bool mesmo = (q == ultimoQuadro) && (agora - ultimoMs < 400);
  ultimoQuadro = q; ultimoMs = agora;
  if (mesmo) return false;

  // Depois do STOP o robo esta morto ate o corte de energia (Artigo 33).
  if (travado) {
    Serial.println("[juiz] comando ignorado: robo em STOP, precisa desligar e ligar");
    return false;
  }

  if (q == JZ_C_NEW || q == JZ_C_OLD) {
    estado = JZ_STOP; travado = true;
    Serial.println("[juiz] STOP (tecla C) - parada definitiva ate reiniciar");
    return true;
  }
  if (q == JZ_A_NEW || q == JZ_A_OLD) {
    estado = JZ_READY;
    Serial.println("[juiz] READY (tecla A) - pronto e imovel");
    return true;
  }
  if (q == JZ_B_NEW || q == JZ_B_OLD) {
    // START so vale saindo de READY. O fluxograma da Figura 6 nao tem
    // caminho do estado inicial direto para START.
    if (estado != JZ_READY) {
      Serial.println("[juiz] START ignorado: o juiz precisa mandar READY antes");
      return false;
    }
    estado = JZ_START;
    Serial.println("[juiz] START (tecla B) - comeca a luta");
    return true;
  }

  Serial.printf("[juiz] tecla nao usada: quadro 0x%08lX (command 0x%02X)\n",
                (unsigned long)q, comandoDe(q));
  return false;
}

const char* nome() {
  switch (estado) {
    case JZ_READY: return "READY";
    case JZ_START: return "START";
    case JZ_STOP:  return "STOP";
    default:       return "espera";
  }
}

} // namespace Juiz
