// =====================================================================
//   ____   ___  ____   ___    ____  _   _ __  __  ___
//  |  _ \ / _ \| __ ) / _ \  / ___|| | | |  \/  |/ _ \
//  | |_) | | | |  _ \| | | | \___ \| | | | |\/| | | | |
//  |  _ <| |_| | |_) | |_| |  ___) | |_| | |  | | |_| |
//  |_| \_\\___/|____/ \___/  |____/ \___/|_|  |_|\___/
//
//  v3 - eletronica enxuta
//  ESP32-C3 SuperMini | 2x motor DC | 1x TB6612FNG | 1x HC-SR04
//  1x IR de borda (saida analogica) | bateria 7,8 V
//
//  UM NUCLEO. O C3 e RISC-V de nucleo unico, entao nao existe "o Wi-Fi
//  mora no outro nucleo". O que garante a reacao na borda e a
//  PRIORIDADE: o FreeRTOS e preemptivo e a guarda de borda tira o HTTP
//  da CPU no meio de um pedido, sem pedir licenca.
//
//      prio 6  guarda de borda   - varre o AO do IR a 1 kHz
//      prio 4  controle          - 200 Hz
//      prio 2  ultrassonico      - ~16 Hz
//      prio 1  Wi-Fi/HTTP e console serial
// =====================================================================

#include "version.h"
#include "config.h"
#include "motors.h"
#include "sensors.h"
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
      if (T.echoUs == 0)   olho = "sem eco: nada na frente, ou modulo mudo";
      else if (T.dist < 0) olho = "eco alem do alcance util";
      else                 olho = "medindo";

      Serial.printf(
        "[OLHO] %4d cm (%s, eco=%u us, falhas=%u)\n"
        "[IR  ] AO=%4u mV  limiar=%u mV -> %s\n"
        "[SYS ] estado=%-9s modo=%-9s pwm=%d/%d loop=%uHz heap=%u\n",
        T.dist, olho, T.echoUs, T.usFail,
        T.irMv, (unsigned)IR_LIMIAR_MV, T.irBorda ? "BORDA" : "seguro",
        STATE_NAME[T.state], MODE_NAME[T.mode], T.pwmL, T.pwmR,
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
      case 'p': Sens::medeSuperficie("piso PRETO"); break;
      case 'b': Sens::medeSuperficie("faixa BRANCA"); break;
      case 'm':
        P.mode = (uint8_t)((P.mode + 1) % MODE_COUNT);
        Brain::applyMode();
        Serial.printf("[cmd] modo -> %s\n", MODE_NAME[P.mode]);
        break;
      case '?':
        Serial.println("[cmd] s=autoteste  a=armar/parar  m=proximo modo");
        Serial.println("[cmd] p=medir piso PRETO  b=medir faixa BRANCA");
        Serial.println("[cmd]   (ponha IR_LIMIAR_MV no meio das duas medidas)");
        break;
      default: break;                     // ignora quebra de linha e digitacao solta
    }
  }
}

// ---------------------------------------------------------------------
//  TASK DE HOUSEKEEPING - console serial
// ---------------------------------------------------------------------
void houseTask(void*) {
  for (;;) {
    pollSerialCmd();
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(400);                      // USB CDC leva um instante para enumerar
  Serial.println("\n================ ROBO SUMO v3 - boot ================");
  Serial.printf("[boot] firmware v%s  (compilado em %s %s)\n", FW_VERSION, __DATE__, __TIME__);

  Web::loadParams();
  Serial.println("[boot] parametros carregados");

  Mot::begin();                    // TB6612FNG nasce em STBY = LOW
  Serial.println("[boot] motores/TB6612FNG prontos (em standby)");

  Sens::begin();
  Serial.println("[boot] sensores (HC-SR04 + IR analogico) prontos");

  Sens::selfTest();

  Brain::begin();
  Serial.println("[boot] maquina de estados pronta");

  Web::begin();
  Serial.println("================ pronto para a luta ================");
  Serial.printf("Wi-Fi: %s (sem senha)\nEndereco: http://%s  ou  http://robosumo.local\n",
                Web::AP_SSID, Web::ipStr);
  Serial.println("A cada segundo este monitor mostra a leitura dos sensores.");
  Serial.println("Atalhos: s=autoteste  a=armar/parar  m=modo  p/b=calibrar IR  (? = ajuda)");
  Serial.println("====================================================\n");

  // ---- tasks -------------------------------------------------------
  // xTaskCreate, e nao PinnedToCore: o C3 tem um nucleo so, e pedir o
  // nucleo 1 aqui dispara um assert do FreeRTOS no boot.
  xTaskCreate(Brain::edgeTask, "edge",    3072, nullptr, 6, nullptr);
  xTaskCreate(controlTask,     "control", 4096, nullptr, 4, nullptr);
  xTaskCreate(Sens::usTask,    "us",      2560, nullptr, 2, nullptr);
  xTaskCreate(Web::task,       "web",     5120, nullptr, 1, nullptr);
  xTaskCreate(houseTask,       "house",   3072, nullptr, 1, nullptr);

  // O robo nasce em ST_IDLE e fica ali ate alguem mandar START, pelo
  // painel web ou pela tecla 'a'. O receptor do edital (GPIO 21) esta
  // reservado para assumir esse papel quando a decodificacao entrar.
  Serial.println("[boot] parado, aguardando START");
}

void loop() {
  // Tudo roda em tasks. A loop() do Arduino so fica fora do caminho.
  vTaskDelay(pdMS_TO_TICKS(500));
}
