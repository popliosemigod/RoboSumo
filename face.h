// =====================================================================
//  ROBO SUMO - face.h
//  Carinha animada no OLED 128x64 (SSD1306, I2C).
//  Roda no core 0 para nao atrapalhar a malha de controle no core 1.
// =====================================================================
#pragma once
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace Face {

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool    ok   = false;
uint8_t addr = 0;          // 0 = nenhum OLED respondeu no barramento

enum Expr : uint8_t {
  EX_SLEEP = 0,  // - _ -   dormindo / desarmado
  EX_IDLE,       // o _ o   acordado, piscando
  EX_COUNT,      // o o     contagem regressiva com o numero gigante
  EX_SEARCH,     // o o     olhos varrendo o horizonte
  EX_LOCK,       // O O     achou! olhos arregalados
  EX_ATTACK,     // > <     modo raiva
  EX_EDGE,       // O O     susto na borda
  EX_DIZZY,      // @ @     tonto (destravando)
  EX_WIN,        // ^ ^     vitoria
  EX_DANCE,      // varia   dancinha
  EX_FAULT,      // x x     falha
  EX_EVIL        // > <     capiroto, com sobrancelhas e chifrinhos
};

Expr    cur = EX_SLEEP;
uint32_t frame = 0;
uint32_t exprSince = 0;
int8_t  lookDir = 0;      // -1 esq, 0 centro, +1 dir
char    banner[18] = "";

inline void set(Expr e) {
  if (cur != e) { cur = e; exprSince = millis(); }
}
inline void look(int8_t d) { lookDir = d; }
inline void say(const char* s) { strncpy(banner, s, sizeof(banner) - 1); banner[sizeof(banner)-1] = 0; }

// ---------------------------------------------------------------------
//  PRIMITIVAS DE OLHO
// ---------------------------------------------------------------------
static void eyeRound(int cx, int cy, int r, float open, int pdx, int pdy, bool fill) {
  int h = (int)(r * open);
  if (h < 2) { oled.drawFastHLine(cx - r, cy, 2 * r, SSD1306_WHITE); return; }
  // olho = retangulo arredondado
  oled.fillRoundRect(cx - r, cy - h, 2 * r, 2 * h, r / 2, SSD1306_WHITE);
  if (fill) return;
  // pupila (buraco preto que se move)
  int pr = r / 2;
  oled.fillCircle(cx + pdx, cy + pdy, pr, SSD1306_BLACK);
}

static void eyeAngry(int cx, int cy, int r, bool right) {
  // desenha ">" ou "<" com tracos grossos
  for (int t = -1; t <= 1; t++) {
    if (right) {   // "<"
      oled.drawLine(cx + r, cy - r + t, cx - r, cy + t, SSD1306_WHITE);
      oled.drawLine(cx - r, cy + t, cx + r, cy + r + t, SSD1306_WHITE);
    } else {       // ">"
      oled.drawLine(cx - r, cy - r + t, cx + r, cy + t, SSD1306_WHITE);
      oled.drawLine(cx + r, cy + t, cx - r, cy + r + t, SSD1306_WHITE);
    }
  }
}

static void eyeHappy(int cx, int cy, int r) {   // "^"
  for (int t = -1; t <= 1; t++) {
    oled.drawLine(cx - r, cy + r + t, cx, cy - r + t, SSD1306_WHITE);
    oled.drawLine(cx, cy - r + t, cx + r, cy + r + t, SSD1306_WHITE);
  }
}

static void eyeX(int cx, int cy, int r) {
  for (int t = -1; t <= 1; t++) {
    oled.drawLine(cx - r, cy - r + t, cx + r, cy + r + t, SSD1306_WHITE);
    oled.drawLine(cx + r, cy - r + t, cx - r, cy + r + t, SSD1306_WHITE);
  }
}

static void eyeSpiral(int cx, int cy, int r, uint32_t f) {
  for (int i = r; i > 1; i -= 3) oled.drawCircle(cx, cy, i, SSD1306_WHITE);
  float a = f * 0.6f;
  oled.fillCircle(cx + (int)(cosf(a) * (r / 2)), cy + (int)(sinf(a) * (r / 2)), 2, SSD1306_WHITE);
}

static void eyeDash(int cx, int cy, int r) {    // "-"
  oled.fillRect(cx - r, cy - 1, 2 * r, 3, SSD1306_WHITE);
}

static void brows(int cy, bool angry) {
  if (angry) {
    oled.drawLine(24, cy - 14, 48, cy - 8, SSD1306_WHITE);
    oled.drawLine(24, cy - 13, 48, cy - 7, SSD1306_WHITE);
    oled.drawLine(104, cy - 14, 80, cy - 8, SSD1306_WHITE);
    oled.drawLine(104, cy - 13, 80, cy - 7, SSD1306_WHITE);
  }
}

static void mouthLine(int cx, int cy, int w)      { oled.fillRect(cx - w/2, cy, w, 2, SSD1306_WHITE); }
static void mouthSmile(int cx, int cy, int w)     { for (int x = -w/2; x <= w/2; x++) { int y = cy + (int)(4 - 16.0f * x * x / (w * w) * 4); oled.drawPixel(cx + x, y, SSD1306_WHITE); oled.drawPixel(cx + x, y+1, SSD1306_WHITE);} }
static void mouthO(int cx, int cy, int r)         { oled.drawCircle(cx, cy + 2, r, SSD1306_WHITE); oled.drawCircle(cx, cy + 2, r - 1, SSD1306_WHITE); }
static void mouthZig(int cx, int cy, int w) {
  int x = cx - w / 2;
  for (int i = 0; i < 6; i++) { oled.drawLine(x, cy + (i % 2 ? 4 : 0), x + w/6, cy + (i % 2 ? 0 : 4), SSD1306_WHITE); x += w/6; }
}

// ---------------------------------------------------------------------
//  BARRA DE STATUS
// ---------------------------------------------------------------------
static void statusBar() {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print(MODE_NAME[T.mode]);
  oled.setCursor(58, 0);
  oled.print(STATE_NAME[T.state]);
  // bateria
  int bx = 112;
  // A barra de bateria saiu: nao ha divisor de bateria neste robo.
  oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);
}

// ---------------------------------------------------------------------
//  TESTE DE BANCADA DO DISPLAY
//
//  Cada etapa procura um defeito diferente - nenhuma delas e enfeite:
//    1. moldura + diagonais: se a moldura sair CORTADA nas laterais, o
//       modulo e SH1106 vendido como SSD1306. Ele tem 132 colunas e so
//       mostra 128, entao a imagem nasce deslocada 2 px - e essa e a
//       assinatura classica. Precisaria de outro driver.
//    2. xadrez de 1 px: revela linha ou coluna morta e contraste ruim.
//    3. tudo aceso: revela pixel queimado e alimentacao fraca - a tela
//       escurece quando a corrente nao acompanha.
//    4. texto: confirma em que endereco ele respondeu.
//
//  Quem desenha continua sendo a task de UI: o pedido so levanta uma
//  janela de tempo. Assim nada mais toca o barramento do display.
// ---------------------------------------------------------------------
uint32_t testStart = 0, testEnd = 0;

inline bool testing() { return testEnd && millis() < testEnd; }
void startTest(uint16_t ms = 6400) { testStart = millis(); testEnd = testStart + ms; }

static void testFrame() {
  uint8_t etapa = (uint8_t)((millis() - testStart) / 1600);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  switch (etapa) {
    case 0:
      oled.drawRect(0, 0, 128, 64, SSD1306_WHITE);
      oled.drawRect(1, 1, 126, 62, SSD1306_WHITE);
      oled.drawLine(0, 0, 127, 63, SSD1306_WHITE);
      oled.drawLine(127, 0, 0, 63, SSD1306_WHITE);
      oled.setTextSize(1);
      oled.setCursor(31, 14); oled.print("MOLDURA");
      oled.setCursor(11, 42); oled.print("cortada=SH1106");
      break;
    case 1:
      for (int y = 0; y < 64; y++)
        for (int x = (y & 1); x < 128; x += 2) oled.drawPixel(x, y, SSD1306_WHITE);
      break;
    case 2:
      oled.fillRect(0, 0, 128, 64, SSD1306_WHITE);
      break;
    default:
      oled.setTextSize(2); oled.setCursor(13, 6); oled.print("OLED OK");
      oled.setTextSize(1);
      oled.setCursor(13, 30); oled.print("SSD1306 128x64");
      oled.setCursor(13, 42); oled.printf("endereco 0x%02X", addr);
      oled.setCursor(13, 54); oled.print("Wire  SDA21 SCL22");
      break;
  }
  oled.display();
}

// ---------------------------------------------------------------------
//  RENDER
// ---------------------------------------------------------------------
void render() {
  if (!ok) return;
  if (testing()) { testFrame(); return; }
  frame++;
  oled.clearDisplay();
  statusBar();

  const int EL = 40, ER = 88;     // centros dos olhos
  int cy = 34;
  uint32_t f = frame;

  // respiracao: a cara sobe e desce de leve
  int bob = (int)(sinf(f * 0.18f) * 1.6f);
  cy += bob;

  int pdx = lookDir * 4;

  switch (cur) {
    case EX_SLEEP: {
      eyeDash(EL, cy, 14); eyeDash(ER, cy, 14);
      mouthLine(64, cy + 18, 16);
      // Zzz flutuante
      oled.setTextSize(1); oled.setCursor(100, 16 + ((f / 8) % 4));
      oled.print("z");
      oled.setCursor(108, 14 + ((f / 6) % 5)); oled.print("Z");
    } break;

    case EX_IDLE: {
      bool blink = ((f % 45) < 3);
      eyeRound(EL, cy, 14, blink ? 0.05f : 1.0f, pdx, 0, false);
      eyeRound(ER, cy, 14, blink ? 0.05f : 1.0f, pdx, 0, false);
      mouthLine(64, cy + 20, 18);
    } break;

    case EX_COUNT: {
      eyeRound(EL, cy - 2, 11, 1.0f, 0, 0, false);
      eyeRound(ER, cy - 2, 11, 1.0f, 0, 0, false);
      oled.setTextSize(3);
      oled.setCursor(56, 42);
      oled.print((int)((T.countdownLeft + 999) / 1000));
      oled.setTextSize(1);
    } break;

    case EX_SEARCH: {
      float s = sinf(f * 0.25f);
      int dx = (int)(s * 6);
      bool blink = ((f % 60) < 3);
      eyeRound(EL, cy, 13, blink ? 0.08f : 0.9f, dx, 0, false);
      eyeRound(ER, cy, 13, blink ? 0.08f : 0.9f, dx, 0, false);
      mouthLine(64, cy + 19, 10);
      oled.setCursor(52, 56); oled.print("? ? ?");
    } break;

    case EX_LOCK: {
      eyeRound(EL, cy, 15, 1.0f, pdx, 0, false);
      eyeRound(ER, cy, 15, 1.0f, pdx, 0, false);
      mouthO(64, cy + 16, 5);
      if ((f / 3) % 2) { oled.setTextSize(2); oled.setCursor(2, 40); oled.print("!"); oled.setCursor(118, 40); oled.print("!"); oled.setTextSize(1); }
    } break;

    case EX_ATTACK: {
      int sh = ((f % 4) < 2) ? 1 : -1;       // tremida de raiva
      eyeAngry(EL + sh, cy, 13, false);
      eyeAngry(ER + sh, cy, 13, true);
      brows(cy, true);
      mouthZig(64, cy + 18, 42);
    } break;

    case EX_EVIL: {
      eyeAngry(EL, cy, 14, false);
      eyeAngry(ER, cy, 14, true);
      brows(cy, true);
      mouthZig(64, cy + 18, 52);
      // chifrinhos
      oled.drawLine(18, 14, 26, 22, SSD1306_WHITE);
      oled.drawLine(18, 14, 22, 20, SSD1306_WHITE);
      oled.drawLine(110, 14, 102, 22, SSD1306_WHITE);
      oled.drawLine(110, 14, 106, 20, SSD1306_WHITE);
      if ((f / 2) % 2) { oled.setCursor(0, 56); oled.print("CAPIROTO"); }
    } break;

    case EX_EDGE: {
      int sh = (f % 2) ? 3 : -3;             // tremendo de susto
      eyeRound(EL + sh, cy, 16, 1.0f, 0, 0, false);
      eyeRound(ER + sh, cy, 16, 1.0f, 0, 0, false);
      mouthO(64 + sh, cy + 17, 7);
      oled.setTextSize(1);
      oled.setCursor(T.irL ? 0 : 92, 56);
      oled.print(T.irL && T.irR ? "!! BORDA !!" : (T.irL ? "<< BORDA" : "BORDA >>"));
    } break;

    case EX_DIZZY: {
      eyeSpiral(EL, cy, 13, f); eyeSpiral(ER, cy, 13, f);
      mouthZig(64, cy + 18, 30);
    } break;

    case EX_WIN: {
      int hop = (int)(fabsf(sinf(f * 0.35f)) * 4);
      eyeHappy(EL, cy - hop, 12); eyeHappy(ER, cy - hop, 12);
      mouthSmile(64, cy + 10 - hop, 40);
      if ((f / 4) % 2) { oled.setCursor(4, 54); oled.print("* VITORIA *"); }
      else             { oled.setCursor(10, 54); oled.print("~ VITORIA ~"); }
    } break;

    case EX_DANCE: {
      uint8_t p = (f / 6) % 4;
      int hop = (int)(fabsf(sinf(f * 0.4f)) * 5);
      int sway = (int)(sinf(f * 0.2f) * 8);
      if (p == 0) { eyeHappy(EL + sway, cy - hop, 12); eyeHappy(ER + sway, cy - hop, 12); }
      else if (p == 1) { eyeRound(EL + sway, cy - hop, 12, 1.0f, 4, 0, false); eyeRound(ER + sway, cy - hop, 12, 1.0f, 4, 0, false); }
      else if (p == 2) { eyeAngry(EL + sway, cy - hop, 12, false); eyeAngry(ER + sway, cy - hop, 12, true); }
      else { eyeRound(EL + sway, cy - hop, 12, 0.1f, 0, 0, true); eyeRound(ER + sway, cy - hop, 12, 1.0f, 0, 0, false); }
      mouthO(64 + sway, cy + 14 - hop, 5);
      const char* notes[4] = { "~ ~ ~", " ~ ~ ~", "~  ~ ~", " ~~ ~ " };
      oled.setCursor(44, 56); oled.print(notes[p]);
    } break;

    case EX_FAULT: {
      eyeX(EL, cy, 13); eyeX(ER, cy, 13);
      mouthLine(64, cy + 18, 24);
      if ((f / 4) % 2) { oled.setCursor(28, 56); oled.print(banner[0] ? banner : "FALHA"); }
    } break;
  }

  if (banner[0] && cur != EX_FAULT) { oled.setCursor(0, 56); oled.print(banner); }
  oled.display();
}

// O begin() da Adafruit devolve true mesmo com o barramento VAZIO - ele so
// falha se nao conseguir alocar o buffer de video. Sem sondar o endereco
// antes, o painel jura que o OLED esta ligado e a gente perde a tarde
// procurando defeito na logica. Mesma tatica que os olhos ja usam.
static bool respondeI2C(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Sobe o display se ele estiver presente. Pode ser chamado de novo em
// tempo de execucao: na bancada o modulo entra e sai do protoboard e nao
// da para resetar a placa a cada fio mexido.
bool probe() {
  addr = respondeI2C(0x3C) ? 0x3C : (respondeI2C(0x3D) ? 0x3D : 0);
  // periphBegin=false: o Wire ja foi iniciado com OS NOSSOS pinos em
  // begin(); deixar a biblioteca reiniciar o barramento faria ela cair
  // nos pinos padrao da placa.
  ok = addr && oled.begin(SSD1306_SWITCHCAPVCC, addr, true, false);
  T.oledOk = ok;
  return ok;
}

bool begin() {
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  if (!probe()) return false;
  Serial.printf("[oled] SSD1306 no endereco 0x%02X\n", addr);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(6, 12);  oled.print("ROBO SUMO");
  oled.setTextSize(1);
  oled.setCursor(30, 40); oled.print("acordando...");
  oled.display();
  return true;
}

// Tela de boas-vindas com o IP do painel
void splash(const char* ip) {
  if (!ok) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);  oled.print("WIFI: ROBO_SUMO");
  oled.setCursor(0, 12); oled.print("SEM SENHA");
  oled.setTextSize(1);
  oled.setCursor(0, 30); oled.print("http://");
  oled.setCursor(42, 30); oled.print(ip);
  oled.setCursor(0, 44); oled.print("http://robosumo.local");
  oled.setCursor(0, 56); oled.print("liga = arma em 5s");
  oled.display();
}

} // namespace Face
