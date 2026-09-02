// =====================================================================
//  ROBO SUMO - sound.h
//  Tocador RTTTL nao bloqueante + bipes proceduais no buzzer passivo.
//
//  As melodias sao dominio publico (Wagner, Rossini, Bizet, Beethoven,
//  toques militares) ou composicoes proprias, transcritas para RTTTL -
//  o mesmo formato dos toques de celular antigos que se acha na internet.
//  Para trocar por outra: cole a string RTTTL em qualquer slot abaixo.
// =====================================================================
#pragma once
#include "config.h"

namespace Snd {

// ---------------------------------------------------------------------
//  BIBLIOTECA DE MELODIAS
// ---------------------------------------------------------------------
// Cavalgada das Valquirias - Wagner (dominio publico) - musica de batalha
const char MEL_BATTLE[] PROGMEM =
  "Valquirias:d=8,o=5,b=132:d,4a,f.,16a,4a,4d6,4a.,p,d,4a,f.,16a,"
  "4a,4d6,4a.,p,d,4a,f.,16a,4d6,4a,4f.,16d,4d,4a,2f.";

// Abertura Guilherme Tell - Rossini (dominio publico) - galope de ataque
const char MEL_GALLOP[] PROGMEM =
  "GuilhermeTell:d=8,o=5,b=200:g,g,g,4g,g,g,4g,g,g,g,c6,e6,4c6,4e6,"
  "g,g,g,4g,g,g,4g,g,g,g,c6,e6,4c6,4g,a,a,a,4a,a,a,4a,g,g,g,g,4g,2c6";

// Toureador (Carmen) - Bizet (dominio publico) - provocacao
const char MEL_TAUNT[] PROGMEM =
  "Toureador:d=8,o=5,b=150:4c6,a,a,4b,4g,g,4c6,c6,4d6,c6,b,4a,4g,4p,"
  "4c6,a,a,4b,4g,4g,4c6,4e6,2c6";

// Toque de carga militar (dominio publico) - vitoria curta
const char MEL_CHARGE[] PROGMEM =
  "Carga:d=4,o=5,b=140:8g,8c6,8e6,16g6,8e6,2g6";

// Hino a Alegria - Beethoven (dominio publico) - vitoria longa
const char MEL_ODE[] PROGMEM =
  "Alegria:d=4,o=5,b=125:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d,"
  "e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c";

// Composicao propria - groove da dancinha da vitoria
const char MEL_DANCE[] PROGMEM =
  "Dancinha:d=16,o=5,b=124:c,p,c,e,g,p,g,e,c,p,c,e,8g,p,a,p,a,c6,e6,p,"
  "e6,c6,a,p,a,c6,8e6,p,f,p,f,a,c6,p,c6,a,f,p,f,a,8c6,p,g,g,8c6,8g,8e,4c";

// Composicao propria - jingle de inicializacao
const char MEL_BOOT[] PROGMEM =
  "Boot:d=16,o=6,b=160:c,e,g,8c7,p,g,8c7";

// Composicao propria - alerta agudo de borda
const char MEL_EDGE[] PROGMEM =
  "Borda:d=32,o=7,b=200:c,a6,c,a6";

// Composicao propria - alvo travado
const char MEL_LOCK[] PROGMEM =
  "Lock:d=32,o=6,b=180:g,c7,e7";

// ---------------------------------------------------------------------
//  ESTADO DO TOCADOR
// ---------------------------------------------------------------------
const char* mel   = nullptr;   // ponteiro PROGMEM da melodia atual
uint16_t    pos   = 0;         // indice dentro da string
uint16_t    start = 0;         // onde comecam as notas
uint8_t     defDur = 4, defOct = 6;
uint16_t    bpm = 120;
uint32_t    wholeMs = 2000;
uint32_t    noteEnd = 0;
bool        playing = false;
bool        looping = false;
uint8_t     volumeDuty = 128;  // duty do LEDC (o "volume" do buzzer passivo)

// fila de bipes simples (usada na contagem regressiva)
struct Beep { uint16_t f; uint16_t ms; };
Beep  beepQ[8];
uint8_t bqN = 0, bqI = 0;

static const uint16_t NOTE_F[] = {
  //  c    c#     d    d#     e     f    f#     g    g#     a    a#     b
     262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494
};

static inline char rd(uint16_t i) { return (char)pgm_read_byte(mel + i); }

static void silence() { pwmTone(PIN_BUZZER, CH_BUZZER, 0); }

void stop() {
  playing = false; looping = false; mel = nullptr;
  bqN = bqI = 0;
  silence();
}

void beep(uint16_t f, uint16_t ms) {
  if (!P.soundOn) return;
  if (bqN >= 8) return;
  beepQ[bqN].f = f; beepQ[bqN].ms = ms; bqN++;
  if (!playing) { noteEnd = 0; }
}

void play(const char* rtttl, bool loop = false) {
  if (!P.soundOn) { stop(); return; }
  mel = rtttl; looping = loop; playing = true;
  bqN = bqI = 0;

  // ---- cabecalho: nome : d=..,o=..,b=.. : notas ----
  defDur = 4; defOct = 6; bpm = 120;
  uint16_t i = 0;
  while (rd(i) && rd(i) != ':') i++;
  if (rd(i) == ':') i++;
  while (rd(i) && rd(i) != ':') {
    char k = rd(i);
    if (k == 'd' || k == 'o' || k == 'b') {
      i++; if (rd(i) == '=') i++;
      uint16_t v = 0;
      while (rd(i) >= '0' && rd(i) <= '9') { v = v * 10 + (rd(i) - '0'); i++; }
      if (k == 'd') defDur = v ? v : 4;
      if (k == 'o') defOct = v ? v : 6;
      if (k == 'b') bpm    = v ? v : 120;
    } else i++;
  }
  if (rd(i) == ':') i++;
  start = pos = i;
  wholeMs = 240000UL / bpm;     // duracao de uma semibreve
  noteEnd = 0;
}

// Avanca para a proxima nota. Retorna false quando a melodia acaba.
static bool nextNote(uint32_t now) {
  while (rd(pos) == ' ' || rd(pos) == ',') pos++;
  if (!rd(pos)) {
    if (looping) { pos = start; while (rd(pos)==' '||rd(pos)==',') pos++; }
    else { playing = false; silence(); return false; }
  }

  // duracao
  uint16_t d = 0;
  while (rd(pos) >= '0' && rd(pos) <= '9') { d = d * 10 + (rd(pos) - '0'); pos++; }
  if (!d) d = defDur;

  // nota
  int8_t n = -1;
  switch (rd(pos)) {
    case 'c': n = 0;  break;  case 'd': n = 2;  break;
    case 'e': n = 4;  break;  case 'f': n = 5;  break;
    case 'g': n = 7;  break;  case 'a': n = 9;  break;
    case 'b': n = 11; break;  case 'p': n = -1; break;
    case 'h': n = 11; break;                       // notacao alema
    default:  n = -1; break;
  }
  pos++;
  if (rd(pos) == '#') { if (n >= 0) n++; pos++; }

  bool dotted = false;
  if (rd(pos) == '.') { dotted = true; pos++; }

  uint8_t oct = defOct;
  if (rd(pos) >= '4' && rd(pos) <= '8') { oct = rd(pos) - '0'; pos++; }
  if (rd(pos) == '.') { dotted = true; pos++; }

  uint32_t dur = wholeMs / d;
  if (dotted) dur += dur / 2;

  if (n < 0) silence();
  else {
    uint32_t f = NOTE_F[n];
    if (oct >= 5) f <<= (oct - 5); else f >>= (5 - oct);
    pwmTone(PIN_BUZZER, CH_BUZZER, f);
  }
  // 88% soando, 12% de respiro -> as notas ficam separadas
  noteEnd = now + dur;
  return true;
}

// Chamar constantemente (a partir da task de interface)
void update() {
  uint32_t now = millis();

  if (bqN) {                             // fila de bipes tem prioridade
    if (now >= noteEnd) {
      if (bqI >= bqN) { bqI = bqN = 0; silence(); }   // fila esvaziou
      else {
        if (beepQ[bqI].f) pwmTone(PIN_BUZZER, CH_BUZZER, beepQ[bqI].f);
        else silence();
        noteEnd = now + beepQ[bqI].ms;
        bqI++;
      }
    }
    return;
  }
  if (!playing || !mel) { return; }
  if (now >= noteEnd) nextNote(now);
}

inline bool busy() { return playing || (bqI < bqN); }

void begin() {
  pwmSetup(PIN_BUZZER, CH_BUZZER, 2000, 10);
  silence();
}

} // namespace Snd
