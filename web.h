// =====================================================================
//  ROBO SUMO v2 - web.h
//  Ponto de acesso proprio (sem senha) + portal cativo + API JSON.
//
//  O painel encolheu de proposito: ele LE o robo e oferece start/stop.
//  Nao ha mais ajuste de parametros pela tela - a afinacao mora no
//  codigo, onde da para revisar e versionar o numero.
// =====================================================================
#pragma once
#include "version.h"
#include "config.h"
#include "brain.h"
#include "juiz.h"
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
    "\"ir\":%u,\"irdo\":%u,"
    "\"pl\":%d,\"pr\":%d,\"flt\":%u,"
    "\"juiz\":%u,\"up\":%lu,\"cd\":%lu,"
    "\"na\":%u,\"ne\":%u,\"nl\":%u,\"hz\":%u,\"ap\":%u,"
    "\"fw\":\"%s\"}",
    T.state, STATE_NAME[T.state], T.mode, MODE_NAME[T.mode], T.armed ? 1 : 0,
    T.dist, T.echoUs, T.usFail, T.confidence,
    T.irBorda ? 1 : 0, T.irDo ? 1 : 0,
    T.pwmL, T.pwmR, T.drvFault ? 1 : 0,
    (unsigned)Juiz::estado, (unsigned long)T.uptimeMs, (unsigned long)T.countdownLeft,
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
  else if (c == "reboot") { srv.send(200, "text/plain", "reiniciando"); delay(150); ESP.restart(); return; }
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
  srv.onNotFound([] {                        // redireciona tudo para o painel
    srv.sendHeader("Location", String("http://") + ipStr + "/", true);
    srv.send(302, "text/plain", "");
  });
  srv.begin();
}

// Tira o ponto de acesso do ar. Durante os rounds o regulamento so
// admite o sinal do controle do juiz (Artigo 3 §3), e usar o proprio
// controle da equipe rende Keikoku (Artigo 41). Com o AP desligado a
// infracao deixa de ser possivel por construcao, e nao por disciplina.
bool noAr = true;
void desliga() {
  if (!noAr) return;
  noAr = false;
  dns.stop();
  srv.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[web] ponto de acesso DESLIGADO para a partida (Artigo 3 §3)");
}

void task(void*) {
  uint32_t dbgT = 0;
  uint8_t  lastN = 255;
  for (;;) {
    if (noAr) {
      dns.processNextRequest();
      srv.handleClient();

      if (millis() - dbgT > 3000) {
        dbgT = millis();
        uint8_t n = WiFi.softAPgetStationNum();
        T.apClients = n;
        if (n != lastN) { Serial.printf("[web] estacoes conectadas ao AP: %u\n", n); lastN = n; }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

} // namespace Web
