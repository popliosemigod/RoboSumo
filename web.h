// =====================================================================
//  ROBO SUMO v4 - web.h
//  Ponto de acesso proprio (sem senha) + portal cativo + API JSON.
//
//  O painel le o robo, aciona os motores e troca de modo. O que ele NAO
//  faz e afinar parametros: slider de ganho no celular parece controle e
//  nao e - o valor que funcionou nao fica versionado nem comentado.
// =====================================================================
#pragma once
#include "version.h"
#include "config.h"
#include "brain.h"
#include "page.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

namespace Web {

WebServer   srv(80);
DNSServer   dns;
Preferences nvs;
char        ipStr[20] = "192.168.4.1";

const char* AP_SSID = "ROBO_SUMO";       // rede aberta, sem senha

// Velocidade da pilotagem manual. Abaixo da de ataque de proposito: na
// bancada o robo anda em cima da mesa, e teto de velocidade ali e queda.
static const int16_t V_MANUAL = 600;

// ---------------------------------------------------------------------
//  NVS
// ---------------------------------------------------------------------
void saveParams() {
  nvs.begin("sumo", false);
  nvs.putBytes("p", &P, sizeof(P));
  nvs.end();
}

void loadParams() {
  P = P_DEFAULT;
  // read-write: no primeiro boot o namespace ainda nao existe e abrir
  // somente-leitura so gera um erro feio no log.
  nvs.begin("sumo", false);
  size_t n = nvs.getBytesLength("p");
  if (n == sizeof(P)) { nvs.getBytes("p", &P, sizeof(P)); Serial.println("[nvs] afinacao salva restaurada"); }
  else                  Serial.println("[nvs] usando afinacao padrao de fabrica");
  nvs.end();
}

// ---------------------------------------------------------------------
//  JSON
// ---------------------------------------------------------------------
char buf[640];

void sendState() {
  snprintf(buf, sizeof(buf),
    "{\"state\":%u,\"stateName\":\"%s\",\"mode\":%u,\"modeName\":\"%s\",\"armed\":%u,"
    "\"d\":%d,\"us\":%u,\"fail\":%u,\"cf\":%u,"
    "\"ir\":%u,\"irmv\":%u,\"lim\":%u,"
    "\"pl\":%d,\"pr\":%d,"
    "\"up\":%lu,\"cd\":%lu,"
    "\"na\":%u,\"ne\":%u,\"nl\":%u,\"hz\":%u,\"ap\":%u,"
    "\"fw\":\"%s\"}",
    T.state, STATE_NAME[T.state], T.mode, MODE_NAME[T.mode], T.armed ? 1 : 0,
    T.dist, T.echoUs, T.usFail, T.confidence,
    T.irBorda ? 1 : 0, T.irMv, (unsigned)IR_LIMIAR_MV,
    T.pwmL, T.pwmR,
    (unsigned long)T.uptimeMs, (unsigned long)T.countdownLeft,
    T.nAttacks, T.nEdgeSaves, T.nLost, T.loopHz, T.apClients,
    FW_VERSION);
  srv.send(200, "application/json", buf);
}

// ---------------------------------------------------------------------
//  /api/cmd
// ---------------------------------------------------------------------
void handleCmd() {
  String c = srv.arg("c");
  if      (c == "arm")    Brain::arm();
  else if (c == "stop")   Brain::disarm();
  else if (c == "save")   saveParams();
  else if (c == "reboot") { srv.send(200, "text/plain", "reiniciando"); delay(150); ESP.restart(); return; }
  srv.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  /api/mode
// ---------------------------------------------------------------------
void handleMode() {
  P.mode = (uint8_t)clampi(srv.arg("m").toInt(), 0, MODE_COUNT - 1);
  Brain::applyMode();
  srv.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  /api/drive  -  acionamento manual, topologia de 2 rodas
//
//  Cinco comandos, que e tudo o que duas rodas permitem: frente, re,
//  girar no proprio eixo para cada lado, e parar.
//
//  O painel REENVIA a direcao a cada 300 ms enquanto o botao esta
//  ativo. Nao e desperdicio: e o que mantem vivo o corte por silencio
//  em ST_MANUAL. Se a aba fechar ou o Wi-Fi cair com o robo andando,
//  ele para sozinho em 700 ms em vez de atravessar a mesa.
// ---------------------------------------------------------------------
void handleDrive() {
  String d = srv.arg("d");
  int16_t v = V_MANUAL;

  if      (d == "fwd")   Brain::manual( v,  v);
  else if (d == "back")  Brain::manual(-v, -v);
  else if (d == "left")  Brain::manual(-v,  v);   // gira no proprio eixo
  else if (d == "right") Brain::manual( v, -v);
  else { Mot::applyNow(0, 0, true); Brain::go(ST_MANUAL); }

  srv.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  BOOT
// ---------------------------------------------------------------------
void begin() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);      // modem-sleep no soft-AP atrasa beacons e derruba clientes
  // softAP() primeiro, softAPConfig() depois - e a ordem que a Espressif
  // documenta; invertida, o IP as vezes nao pega no primeiro boot.
  bool apOk = WiFi.softAP(AP_SSID);          // sem senha, de proposito
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  strncpy(ipStr, WiFi.softAPIP().toString().c_str(), sizeof(ipStr) - 1);
  Serial.printf("[web] softAP %s | IP %s\n", apOk ? "OK" : "FALHOU", ipStr);

  dns.start(53, "*", WiFi.softAPIP());       // portal cativo: qualquer URL cai no painel
  if (MDNS.begin("robosumo")) MDNS.addService("http", "tcp", 80);

  srv.on("/", HTTP_GET, [] {
    srv.sendHeader("Cache-Control", "no-store");
    srv.send_P(200, "text/html", PAGE_HTML);
  });
  srv.on("/api/state", HTTP_GET, sendState);
  srv.on("/api/cmd",   HTTP_GET, handleCmd);
  srv.on("/api/mode",  HTTP_GET, handleMode);
  srv.on("/api/drive", HTTP_GET, handleDrive);
  srv.onNotFound([] {                        // redireciona tudo para o painel
    srv.sendHeader("Location", String("http://") + ipStr + "/", true);
    srv.send(302, "text/plain", "");
  });
  srv.begin();
}

void task(void*) {
  uint32_t dbgT = 0;
  uint8_t  lastN = 255;
  for (;;) {
    dns.processNextRequest();
    srv.handleClient();

    if (millis() - dbgT > 3000) {
      dbgT = millis();
      uint8_t n = WiFi.softAPgetStationNum();
      T.apClients = n;
      if (n != lastN) { Serial.printf("[web] estacoes conectadas ao AP: %u\n", n); lastN = n; }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

} // namespace Web
