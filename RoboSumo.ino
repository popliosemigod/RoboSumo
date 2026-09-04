// =====================================================================
//   ____   ___  ____   ___    ____  _   _ __  __  ___
//  |  _ \ / _ \| __ ) / _ \  / ___|| | | |  \/  |/ _ \
//  | |_) | | | |  _ \| | | | \___ \| | | | |\/| | | | |
//  |  _ <| |_| | |_) | |_| |  ___) | |_| | |  | | |_| |
//  |_| \_\\___/|____/ \___/  |____/ \___/|_|  |_|\___/
//
//  ESP32 | 4x N20 6V | 2x DRV8833 | 2x VL53L0X | 2x IR | OLED | buzzer
//
//  Distribuicao entre os nucleos:
//    core 1 -> guarda de borda (prio 6), controle (prio 4), ToF (prio 2)
//    core 0 -> Wi-Fi/HTTP (prio 2), OLED + buzzer (prio 1)
//  Assim o Wi-Fi nunca engasga a malha que salva o robo da borda.
//
//  Bibliotecas necessarias: Adafruit GFX + Adafruit SSD1306
//  Placa: "ESP32 Dev Module"
// =====================================================================

#include "version.h"
#include "config.h"
#include "motors.h"
#include "sensors.h"
#include "sound.h"
#include "face.h"
#include "brain.h"
#include "web.h"

// ---- definicao dos globais declarados em config.h --------------------
Params    P;
Telemetry T;
Event     EVLOG[EVT_MAX];
uint8_t   EVN = 0, EVI = 0;

// Monitor de IR: 20 Hz por alguns segundos, para enxergar a FORMA do
// sinal. A 1 Hz do heartbeat nao da para separar "modulo reagindo ao papel"
// de "pino solto derivando" - os dois so mostram numeros diferentes.
volatile uint32_t irMonUntil = 0;

// Mesma ideia para os olhos. Aqui a taxa importa por outro motivo: o que
// mata a investida nao e a distancia errada, e a distancia INSTAVEL - uma
// leitura que pula sozinha faz o PID corrigir para um alvo que nao existe.
// A 20 Hz da para ver o tremor; a 1 Hz do heartbeat, nao.
volatile uint32_t tofMonUntil = 0;

void evPush(uint8_t type, int16_t a, int16_t b) {
  EVLOG[EVI].t    = millis() - Brain::armedAt;
  EVLOG[EVI].type = type;
  EVLOG[EVI].a    = a;
  EVLOG[EVI].b    = b;
  EVI = (EVI + 1) % EVT_MAX;
  if (EVN < EVT_MAX) EVN++;
}

// ---------------------------------------------------------------------
//  TASK DE CONTROLE  -  core 1, 200 Hz
// ---------------------------------------------------------------------
void controlTask(void*) {
  TickType_t last = xTaskGetTickCount();
  uint32_t hzT = 0, hzN = 0;
  uint32_t dbgT = 0;
  for (;;) {
    Sens::pollIrAnalog();          // redundancia analogica do IR
    Brain::tick();

    if (irMonUntil) {
      static uint32_t lastMon = 0;
      if ((int32_t)(millis() - irMonUntil) >= 0) {
        irMonUntil = 0;
        Serial.println("#IRFIM");
      } else if (millis() - lastMon >= 50) {
        lastMon = millis();
        Serial.printf("#IR %lu %u %u %u %u\n", (unsigned long)millis(),
                      T.irLraw, T.irRraw,
                      digitalRead(PIN_IR_L_D) ? 1 : 0, digitalRead(PIN_IR_R_D) ? 1 : 0);
      }
    }

    if (tofMonUntil) {
      static uint32_t lastTof = 0;
      if ((int32_t)(millis() - tofMonUntil) >= 0) {
        tofMonUntil = 0;
        Serial.println("#TOFFIM");
      } else if (millis() - lastTof >= 50) {
        lastTof = millis();
        Serial.printf("#TOF %lu %d %d %d %d %u %u %u %u\n", (unsigned long)millis(),
                      T.tofRawL, T.tofRawR, T.distL, T.distR,
                      T.tofOkL ? 1 : 0, T.tofOkR ? 1 : 0, T.tofFailL, T.tofFailR);
      }
    }

    hzN++;
    if (millis() - hzT >= 1000) { T.loopHz = hzN; hzN = 0; hzT = millis(); }

    // Heartbeat de sensores pela Serial - funciona mesmo se o painel web
    // estiver fora do ar, entao da para calibrar/depurar so com o cabo USB.
    if (millis() - dbgT >= 1000) {
      dbgT = millis();
      auto tofTxt = [](bool presente, int16_t mm) -> const char* {
        if (!presente) return "AUSENTE no barramento";
        if (mm < 0)    return "sem alvo no alcance";
        return "medindo";
      };
      Serial.printf(
        "[OLHO] esq: %4dcm (%s, cru=%dmm, falhas=%u) | dir: %4dcm (%s, cru=%dmm, falhas=%u)\n"
        "[IR  ] esq: pino=%s AO=%4u -> %s | dir: pino=%s AO=%4u -> %s\n"
        "[SYS ] estado=%-9s vbat=%.2fV loop=%uHz heap=%u\n",
        T.distL, tofTxt(T.tofOkL, T.tofRawL), T.tofRawL, T.tofFailL,
        T.distR, tofTxt(T.tofOkR, T.tofRawR), T.tofRawR, T.tofFailR,
        digitalRead(PIN_IR_L_D) ? "ALTO" : "BAIXO", T.irLraw, T.irL ? "BORDA" : "seguro",
        digitalRead(PIN_IR_R_D) ? "ALTO" : "BAIXO", T.irRraw, T.irR ? "BORDA" : "seguro",
        STATE_NAME[T.state], T.vbat / 100.0f, T.loopHz, ESP.getFreeHeap());

      // Linha legivel por maquina, consumida pelo painel de bancada do PC
      // (docs\painel.ps1). Prefixo "#D" para separar do texto humano.
      Serial.printf(
        "#D st=%u armed=%u mode=%u hz=%u heap=%u oled=%u ap=%u drv=%u bus=%u "
        "tofL=%u tofR=%u dL=%d dR=%d rL=%d rR=%d fL=%u fR=%u "
        "irLd=%u irRd=%u irLa=%u irRa=%u irL=%u irR=%u vbat=%u pwmL=%d pwmR=%d\n",
        T.state, T.armed ? 1 : 0, T.mode, T.loopHz, ESP.getFreeHeap(),
        T.oledOk ? 1 : 0, T.apClients, T.drvFault ? 1 : 0, T.tofNoBus ? 1 : 0,
        T.tofOkL ? 1 : 0, T.tofOkR ? 1 : 0, T.distL, T.distR,
        T.tofRawL, T.tofRawR, T.tofFailL, T.tofFailR,
        digitalRead(PIN_IR_L_D) ? 1 : 0, digitalRead(PIN_IR_R_D) ? 1 : 0,
        T.irLraw, T.irRraw, T.irL ? 1 : 0, T.irR ? 1 : 0,
        T.vbat, T.pwmL, T.pwmR);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(5));
  }
}

// ---------------------------------------------------------------------
//  TASK DE INTERFACE  -  core 0: buzzer sempre, OLED a ~16 fps
// ---------------------------------------------------------------------
void uiTask(void*) {
  uint32_t lastFace = 0;
  for (;;) {
    Snd::update();     // buzzer nao usa I2C: toca mesmo durante o diagnostico
    if ((P.faceOn || Face::testing()) && !Sens::diagBusy && millis() - lastFace >= 60) {
      lastFace = millis();
      Face::render();
    }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

// ---------------------------------------------------------------------
//  RAJADA DE IR - 256 amostras a 1 kHz
//
//  A 20 Hz so se ve alias. Se o AO estiver oscilando sozinho, o suspeito
//  numero um e a luz da sala: lampada fluorescente ou LED barato pisca a
//  100 ou 120 Hz, e o fototransistor do modulo IR enxerga isso muito bem.
//  A 1 kHz a onda aparece inteira e da para contar a frequencia.
//
//  Roda no core 0 (task de housekeeping), entao nao atrasa a guarda de
//  borda nem a malha de controle, que vivem no core 1.
// ---------------------------------------------------------------------
void rajadaIR() {
  const uint16_t N = 256;
  static uint16_t bufL[256], bufR[256];
  Serial.println("#RAJADA ini taxa=1000Hz n=256");
  uint32_t t0 = micros();
  for (uint16_t i = 0; i < N; i++) {
    bufL[i] = analogRead(PIN_IR_L_A);
    bufR[i] = analogRead(PIN_IR_R_A);
    while ((int32_t)(micros() - t0 - (uint32_t)(i + 1) * 1000) < 0) { }
  }
  for (uint16_t i = 0; i < N; i++) Serial.printf("#B %u %u %u\n", i, bufL[i], bufR[i]);
  Serial.println("#RAJADA fim");
}

// ---------------------------------------------------------------------
//  CONSOLE SERIAL - atalhos de bancada
//
//  Existe porque durante a montagem o painel web nem sempre esta a mao:
//  com o cabo USB ja plugado, apertar uma tecla vale mais do que pegar o
//  celular, entrar no AP do robo e procurar o botao na tela.
// ---------------------------------------------------------------------
void pollSerialCmd() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 't':
        Serial.println("[cmd] padrao de teste no display (6,4 s)");
        Face::startTest();
        break;
      case 's':
        Sens::selfTest();
        if (!T.oledOk) Face::probe();     // pega display plugado com a placa ligada
        break;
      case 'b':
        Serial.println("[cmd] bipe");
        Snd::play(Snd::MEL_BOOT);
        break;
      case 'd': Sens::calEscuro(); break;    // robo na zona escura
      case 'c': Sens::calClaro();  break;    // robo em cima da faixa clara
      case 'x': Sens::calLimpa();  break;
      case 'm':
        irMonUntil = millis() + 20000;    // 20 s de monitor a 20 Hz
        Serial.println("#IRINI");
        break;
      case 'o':
        tofMonUntil = millis() + 20000;
        Serial.println("#TOFINI");
        break;
      case 'e': Sens::exameOlhos(); break;
      case 'w': Sens::testeWire1(); break;
      case 'f': rajadaIR(); break;
      case 'i': Sens::i2cNaUnha(); break;
      case 'k': Sens::sondaModulo(); break;
      case 'u': Sens::destravaBarramentos(); break;
      case 'a':
        if (T.armed) Brain::disarm(); else Brain::arm();
        break;
      case '?':
        Serial.println("[cmd] t=display  s=autoteste  b=buzzer  a=armar/parar");
        Serial.println("[cmd] d=capturar ESCURO  c=capturar CLARO  x=zerar calibracao");
        Serial.println("[cmd] m=monitor de IR a 20 Hz  o=monitor dos OLHOS a 20 Hz");
        Serial.println("[cmd] e=exame dos olhos (XSHUT, velocidade, pinos trocados)");
        Serial.println("[cmd] k=mede VCC/GND NO PINO do modulo (GPIO 32 e 33)");
        Serial.println("[cmd] u=destrava os dois barramentos I2C e varre");
        break;
      default: break;                     // ignora quebra de linha e digitacao solta
    }
  }
}

// ---------------------------------------------------------------------
//  TASK DE HOUSEKEEPING - core 0: bateria, botao fisico e console serial
// ---------------------------------------------------------------------
void houseTask(void*) {
  uint32_t pressAt = 0; bool longDone = false;
  for (;;) {
    Sens::pollVbat();
    pollSerialCmd();

    bool down = (digitalRead(PIN_BTN) == LOW);
    if (down && !pressAt) { pressAt = millis(); longDone = false; }
    if (down && pressAt && !longDone && millis() - pressAt > 1200) {
      // pressao longa: troca de modo
      P.mode = (P.mode + 1) % MODE_COUNT;
      Brain::applyMode();
      Snd::beep(900 + P.mode * 180, 90); Snd::beep(0, 60);
      Face::say(MODE_NAME[P.mode]);
      longDone = true;
    }
    if (!down && pressAt) {
      if (!longDone) {                       // toque curto: arma / desarma
        if (T.armed) Brain::disarm(); else Brain::arm();
      }
      pressAt = 0;
    }

    digitalWrite(PIN_LED, T.armed ? ((millis() / 200) % 2) : LOW);
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(60);
  Serial.println("\n================ ROBO SUMO - boot ================");
  Serial.printf("[boot] firmware v%s  (compilado em %s %s)\n", FW_VERSION, __DATE__, __TIME__);

  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  Web::loadParams();               // parametros salvos na NVS (ou padrao)
  Serial.println("[boot] parametros carregados");

  Mot::begin();                    // pontes H nascem em nSLEEP = LOW
  Serial.println("[boot] motores/DRV8833 prontos (dormindo)");

  Snd::begin();
  Serial.println("[boot] buzzer pronto");

  Sens::begin();
  Serial.println("[boot] sensores (VL53L0X + IR) prontos");

  bool haveOled = Face::begin();     // e aqui que Wire (do OLED) e iniciado
  Serial.printf("[boot] OLED: %s\n", haveOled ? "encontrado" : "NAO ENCONTRADO (checar SDA=21 SCL=22 e alimentacao)");

  Sens::selfTest();                  // varre os dois barramentos ja iniciados

  Brain::begin();
  Serial.println("[boot] maquina de estados pronta");

  Web::begin();
  Serial.println("================ pronto para a luta ================");
  Serial.printf("Wi-Fi: %s (sem senha)\nEndereco: http://%s  ou  http://robosumo.local\n",
                Web::AP_SSID, Web::ipStr);
  Serial.println("A cada segundo este monitor vai mostrar a leitura dos sensores.");
  Serial.println("Atalhos por aqui: t=display  s=autoteste  b=buzzer  a=armar/parar");
  Serial.println("Calibrar IR:      d=capturar ESCURO  c=capturar CLARO  x=zerar  (? = ajuda)");
  Serial.println("====================================================\n");

  Face::splash(Web::ipStr);
  Snd::play(Snd::MEL_BOOT);
  delay(1400);

  // ---- tasks -------------------------------------------------------
  xTaskCreatePinnedToCore(Brain::edgeTask, "edge",    3072, nullptr, 6, nullptr, 1);
  xTaskCreatePinnedToCore(controlTask,     "control", 6144, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(Sens::tofTask,   "tof",     4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(Web::task,       "web",     8192, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(uiTask,          "ui",      6144, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(houseTask,       "house",   3072, nullptr, 1, nullptr, 0);
}

void loop() {
  // Tudo roda em tasks. A loop() do Arduino so fica fora do caminho.
  vTaskDelay(pdMS_TO_TICKS(500));
}
