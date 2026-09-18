// =====================================================================
//   ____   ___  ____   ___    ____  _   _ __  __  ___
//  |  _ \ / _ \| __ ) / _ \  / ___|| | | |  \/  |/ _ \
//  | |_) | | | |  _ \| | | | \___ \| | | | |\/| | | | |
//  |  _ <| |_| | |_) | |_| |  ___) | |_| | |  | | |_| |
//  |_| \_\\___/|____/ \___/  |____/ \___/|_|  |_|\___/
//
//  v2 - eletronica simplificada
//  ESP32-C3 Mini | 4x motor DC | 2x DRV8833 | 1x HC-SR04 | 1x IR de borda
//
//  UM NUCLEO. Este e o ponto que mais muda em relacao a versao do ESP32
//  DevKit: o C3 e RISC-V de nucleo unico, entao nao existe mais "o Wi-Fi
//  mora no outro nucleo". Todo xTaskCreatePinnedToCore(..., 1, ...) que
//  havia aqui quebraria em tempo de execucao.
//
//  O que garante a reacao na borda agora e a PRIORIDADE, nao o nucleo. O
//  FreeRTOS e preemptivo: a guarda de borda (prio 6) tira o HTTP (prio 1)
//  da CPU no meio de um pedido, sem pedir licenca.
//
//      prio 6  guarda de borda   - acordada pela ISR do IR
//      prio 4  controle          - 200 Hz
//      prio 2  ultrassonico      - ~16 Hz
//      prio 1  Wi-Fi/HTTP, juiz e console serial
// =====================================================================

#include "version.h"
#include "config.h"
#include "motors.h"
#include "sensors.h"
#include "juiz.h"
#include "brain.h"
#include "web.h"

// ---- definicao dos globais declarados em config.h --------------------
Params    P;
Telemetry T;

// ---------------------------------------------------------------------
//  TASK DE CONTROLE  -  200 Hz
// ---------------------------------------------------------------------
void controlTask(void*) {
  TickType_t last = xTaskGetTickCount();
  uint32_t hzN = 0, hzT = 0, dbgT = 0;

  for (;;) {
    Sens::pollIr();          // ressincroniza a borda (a ISR so pega transicao)
    Brain::tick();

    hzN++;
    if (millis() - hzT >= 1000) { T.loopHz = hzN; hzN = 0; hzT = millis(); }

    // Heartbeat pela Serial - funciona mesmo com o Wi-Fi fora do ar,
    // entao da para depurar so com o cabo USB.
    if (millis() - dbgT >= 1000) {
      dbgT = millis();

      // O motivo da leitura invalida importa: "nada na frente" e um
      // sensor saudavel, "nenhum eco nunca" e um sensor que nao esta
      // conversando. O eco cru separa os dois.
      const char* olho;
      if (T.echoUs == 0)          olho = "sem eco: nada na frente, ou modulo mudo";
      else if (T.dist < 0)        olho = "eco alem do alcance util";
      else                        olho = "medindo";

      Serial.printf(
        "[OLHO] %4d cm (%s, eco=%u us, falhas=%u)\n"
        "[IR  ] pino=%-5s -> %s\n"
        "[SYS ] estado=%-9s juiz=%-7s pwm=%d/%d loop=%uHz heap=%u\n",
        T.dist, olho, T.echoUs, T.usFail,
        T.irDo ? "ALTO" : "BAIXO", T.irBorda ? "BORDA" : "seguro",
        STATE_NAME[T.state], Juiz::nome(), T.pwmL, T.pwmR,
        T.loopHz, (unsigned)ESP.getFreeHeap());
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(5));
  }
}

// ---------------------------------------------------------------------
//  CONSOLE SERIAL - atalhos de bancada
// ---------------------------------------------------------------------
void pollSerialCmd() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 's': Sens::selfTest(); break;
      case 'a': if (T.armed) Brain::disarm(); else Brain::arm(); break;
      case '?':
        Serial.println("[cmd] s=autoteste  a=armar/parar  ?=ajuda");
        break;
      default: break;                     // ignora quebra de linha e digitacao solta
    }
  }
}

// ---------------------------------------------------------------------
//  TASK DE HOUSEKEEPING - juiz e console serial
// ---------------------------------------------------------------------
void houseTask(void*) {
  for (;;) {
    pollSerialCmd();

    if (Juiz::poll()) {
      switch (Juiz::estado) {
        case Juiz::JZ_READY:
          // "pronto e imovel". Se ja estava lutando, isto e a pausa do juiz.
          if (T.armed) Brain::disarm();
          // Artigo 3 §3: durante a partida o unico sinal externo permitido
          // e o do controle do juiz. O AP sai do ar aqui e so volta com o
          // robo reiniciado.
          Web::desliga();
          break;
        case Juiz::JZ_START:
          if (!T.armed) Brain::arm();
          break;
        case Juiz::JZ_STOP:
          Brain::disarm();
          break;
        default: break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(400);                      // USB CDC leva um instante para enumerar
  Serial.println("\n================ ROBO SUMO v2 - boot ================");
  Serial.printf("[boot] firmware v%s  (compilado em %s %s)\n", FW_VERSION, __DATE__, __TIME__);

  Web::loadParams();
  Serial.println("[boot] parametros carregados");

  Mot::begin();                    // pontes H nascem em nSLEEP = LOW
  Serial.println("[boot] motores/DRV8833 prontos (dormindo)");

  Sens::begin();
  Serial.println("[boot] sensores (HC-SR04 + IR de borda) prontos");

  Juiz::begin();

  Sens::selfTest();

  Brain::begin();
  Serial.println("[boot] maquina de estados pronta");

  Web::begin();
  Serial.println("================ pronto para a luta ================");
  Serial.printf("Wi-Fi: %s (sem senha)\nEndereco: http://%s  ou  http://robosumo.local\n",
                Web::AP_SSID, Web::ipStr);
  Serial.println("A cada segundo este monitor mostra a leitura dos sensores.");
  Serial.println("Atalhos por aqui: s=autoteste  a=armar/parar  (? = ajuda)");
  Serial.println("====================================================\n");

  // ---- tasks -------------------------------------------------------
  // xTaskCreate, e nao PinnedToCore: o C3 tem um nucleo so, e pedir o
  // nucleo 1 aqui dispara um assert do FreeRTOS no boot.
  xTaskCreate(Brain::edgeTask, "edge",    3072, nullptr, 6, nullptr);
  xTaskCreate(controlTask,     "control", 4096, nullptr, 4, nullptr);
  xTaskCreate(Sens::usTask,    "us",      2560, nullptr, 2, nullptr);
  xTaskCreate(Web::task,       "web",     5120, nullptr, 1, nullptr);
  xTaskCreate(houseTask,       "house",   3072, nullptr, 1, nullptr);

  // Quem arma e o juiz. O robo nasce em ST_IDLE e fica ali ate o START.
  Serial.println("[boot] aguardando o juiz: A=Ready  B=Start  C=Stop");
}

void loop() {
  // Tudo roda em tasks. A loop() do Arduino so fica fora do caminho.
  vTaskDelay(pdMS_TO_TICKS(500));
}
