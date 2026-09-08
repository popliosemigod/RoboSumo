// =====================================================================
//  ROBO SUMO - web.h
//  Ponto de acesso proprio (sem senha) + portal cativo + API JSON +
//  gravacao OTA do firmware. Nenhuma biblioteca externa.
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
#include <Update.h>
#include <Preferences.h>

namespace Web {

WebServer   srv(80);
DNSServer   dns;
Preferences nvs;
char        ipStr[20] = "192.168.4.1";
bool        otaOk = false;

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
void resetParams() { P = P_DEFAULT; saveParams(); Brain::applyMode(); }

// ---------------------------------------------------------------------
//  JSON
// ---------------------------------------------------------------------
char buf[2048];

void sendState() {
  snprintf(buf, sizeof(buf),
    "{\"state\":%u,\"stateName\":\"%s\",\"mode\":%u,\"modeName\":\"%s\",\"armed\":%u,"
    "\"dl\":%d,\"dr\":%d,\"df\":%d,\"bear\":%d,\"cf\":%u,"
    "\"irL\":%u,\"irR\":%u,\"irLr\":%u,\"irRr\":%u,"
    "\"irLd\":%u,\"irRd\":%u,\"tofL\":%d,\"tofR\":%d,\"tofFL\":%u,\"tofFR\":%u,"
    "\"tofOkL\":%u,\"tofOkR\":%u,"
    "\"pl\":%d,\"pr\":%d,\"pp\":%.1f,\"pi\":%.1f,\"pd\":%.1f,\"po\":%.1f,"
    "\"juiz\":%u,\"flt\":%u,\"up\":%lu,\"cd\":%lu,"
    "\"na\":%u,\"ne\":%u,\"nl\":%u,\"ta\":%lu,\"hz\":%u,"
    "\"cEsc\":%u,\"cEscL\":%u,\"cEscR\":%u,\"cCla\":%u,\"cClaL\":%u,\"cClaR\":%u,\"cMar\":%d,"
    "\"fw\":\"%s\"}",
    T.state, STATE_NAME[T.state], T.mode, MODE_NAME[T.mode], T.armed ? 1 : 0,
    T.distL, T.distR, T.distFused, T.bearing, T.confidence,
    T.irL ? 1 : 0, T.irR ? 1 : 0, T.irLraw, T.irRraw,
    T.irLdo ? 1 : 0, T.irRdo ? 1 : 0, T.tofRawL, T.tofRawR, T.tofFailL, T.tofFailR,
    T.tofOkL ? 1 : 0, T.tofOkR ? 1 : 0,
    T.pwmL, T.pwmR, T.pidP, T.pidI, T.pidD, T.pidOut,
    (unsigned)Juiz::estado, T.drvFault ? 1 : 0, (unsigned long)T.uptimeMs, (unsigned long)T.countdownLeft,
    T.nAttacks, T.nEdgeSaves, T.nLost, (unsigned long)T.timeAttackingMs, T.loopHz,
    Sens::CAL.temEsc ? 1 : 0, Sens::CAL.escL, Sens::CAL.escR,
    Sens::CAL.temCla ? 1 : 0, Sens::CAL.claL, Sens::CAL.claR, Sens::CAL.margem,
    FW_VERSION);
  srv.send(200, "application/json", buf);
}

void sendParams() {
  snprintf(buf, sizeof(buf),
    "{\"vSearch\":%u,\"vAttack\":%u,\"vMax\":%u,\"vReverse\":%u,\"motInvL\":%u,\"motInvR\":%u,"
    "\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,"
    "\"rangeCm\":%u,\"confirmHits\":%u,\"loseMisses\":%u,\"jumpCm\":%u,"
    "\"edgeBackMs\":%u,\"edgeTurnMs\":%u,\"irThreshold\":%u,\"irSource\":%u,\"irActiveLow\":%u,"
    "\"sweepMs\":%u,\"rampMs\":%u,\"stuckMs\":%u,\"countdownMs\":%u,"
    "\"mode\":%u,\"soundOn\":%u,\"faceOn\":%u,\"autoRestart\":%u}",
    P.vSearch, P.vAttack, P.vMax, P.vReverse, P.motInvL, P.motInvR, P.kp, P.ki, P.kd,
    P.rangeCm, P.confirmHits, P.loseMisses, P.jumpCm,
    P.edgeBackMs, P.edgeTurnMs, P.irThreshold, P.irSource, P.irActiveLow,
    P.sweepMs, P.rampMs, P.stuckMs, P.countdownMs,
    P.mode, P.soundOn, P.faceOn, P.autoRestart);
  srv.send(200, "application/json", buf);
}

void sendLog() {
  String s = "[";
  for (uint8_t i = 0; i < EVN; i++) {
    uint8_t k = (EVN < EVT_MAX) ? i : (uint8_t)((EVI + i) % EVT_MAX);
    if (i) s += ',';
    s += "{\"t\":"; s += EVLOG[k].t;
    s += ",\"y\":";  s += EVLOG[k].type;
    s += ",\"a\":";  s += EVLOG[k].a;
    s += ",\"b\":";  s += EVLOG[k].b;
    s += '}';
  }
  s += ']';
  srv.send(200, "application/json", s);
}

// ---------------------------------------------------------------------
//  /api/selftest  -  varre os dois barramentos I2C AGORA e devolve o que
//  respondeu. E o mesmo autoteste do boot, mas sob demanda: da para
//  mexer num fio e conferir pelo celular, sem cabo USB e sem resetar.
// ---------------------------------------------------------------------
static String lista(const uint8_t* a, uint8_t n) {
  String s = "[";
  for (uint8_t i = 0; i < n && i < 8; i++) { if (i) s += ','; s += a[i]; }
  return s + ']';
}

void sendSelfTest() {
  Sens::selfTest();
  // Se o OLED acabou de aparecer no barramento, sobe ele na hora: na
  // bancada o modulo entra e sai do protoboard o tempo todo.
  if (!T.oledOk) Face::probe();

  String s = "{\"at\":";            s += Sens::DG.at;
  s += ",\"busOled\":";             s += lista(Sens::DG.aOled, Sens::DG.nOled);
  s += ",\"busTof\":";              s += lista(Sens::DG.aTof,  Sens::DG.nTof);
  s += ",\"oled\":";                s += T.oledOk ? 1 : 0;
  s += ",\"oledAddr\":";            s += Face::addr;
  s += ",\"tofL\":";                s += T.tofOkL ? 1 : 0;
  s += ",\"tofR\":";                s += T.tofOkR ? 1 : 0;
  s += ",\"irLdo\":";               s += Sens::DG.irLdo ? 1 : 0;
  s += ",\"irRdo\":";               s += Sens::DG.irRdo ? 1 : 0;
  s += ",\"irLa\":";                s += Sens::DG.irLa;
  s += ",\"irRa\":";                s += Sens::DG.irRa;
  s += ",\"flt\":";                 s += T.drvFault ? 1 : 0;
  s += ",\"hz\":";                  s += T.loopHz;
  s += ",\"heap\":";                s += (uint32_t)ESP.getFreeHeap();
  s += ",\"ap\":";                  s += T.apClients;
  s += ",\"fw\":\"";                s += FW_VERSION; s += "\"}";
  srv.send(200, "application/json", s);
}

// ---------------------------------------------------------------------
//  /api/set  -  aceita varias chaves de uma vez
// ---------------------------------------------------------------------
#define SETU(k, field, lo, hi) if (srv.hasArg(k)) P.field = (decltype(P.field))clampi(srv.arg(k).toInt(), lo, hi);

void handleSet() {
  SETU("vSearch",     vSearch,      0, 1000)
  SETU("vAttack",     vAttack,      0, 1000)
  SETU("vMax",        vMax,       100, 1000)
  SETU("vReverse",    vReverse,     0, 1000)
  SETU("motInvL",     motInvL,      0,    1)
  SETU("motInvR",     motInvR,      0,    1)
  SETU("rangeCm",     rangeCm,     10,  400)
  SETU("confirmHits", confirmHits,  1,   30)
  SETU("loseMisses",  loseMisses,   1,   60)
  SETU("jumpCm",      jumpCm,       2,  200)
  SETU("edgeBackMs",  edgeBackMs,  20, 2000)
  SETU("edgeTurnMs",  edgeTurnMs,  20, 2000)
  SETU("irThreshold", irThreshold,  0, 4095)
  SETU("irSource",    irSource,     0,    2)
  SETU("irActiveLow", irActiveLow,  0,    1)
  SETU("sweepMs",     sweepMs,    100, 5000)
  SETU("rampMs",      rampMs,       0, 2000)
  SETU("stuckMs",     stuckMs,    100, 9000)
  SETU("countdownMs", countdownMs,  0, 20000)
  SETU("soundOn",     soundOn,      0,    1)
  SETU("faceOn",      faceOn,       0,    1)
  SETU("autoRestart", autoRestart,  0,    1)
  if (srv.hasArg("kp")) P.kp = constrain(srv.arg("kp").toFloat(), 0.0f, 50.0f);
  if (srv.hasArg("ki")) P.ki = constrain(srv.arg("ki").toFloat(), 0.0f, 10.0f);
  if (srv.hasArg("kd")) P.kd = constrain(srv.arg("kd").toFloat(), 0.0f, 50.0f);
  Brain::applyMode();
  srv.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  /api/cmd
// ---------------------------------------------------------------------
void handleCmd() {
  String c = srv.arg("c");
  if      (c == "arm")   Brain::arm();
  else if (c == "stop")  Brain::disarm();
  else if (c == "dance") { Brain::applyMode(); T.armed = true; Mot::wake(true); Brain::startDance(); }
  else if (c == "save")  saveParams();
  else if (c == "reset") resetParams();
  else if (c == "beep")  { Snd::play(Snd::MEL_BOOT); }
  else if (c == "taunt") { Snd::play(Snd::MEL_TAUNT); }
  else if (c == "testoled") Face::startTest();
  else if (c == "irEscuro") Sens::calEscuro();
  else if (c == "irClaro")  Sens::calClaro();
  else if (c == "irLimpa")  Sens::calLimpa();
  else if (c == "testL") { Brain::manual(500, 0); }
  else if (c == "testR") { Brain::manual(0, 500); }
  else if (c == "reboot") { srv.send(200, "text/plain", "reiniciando"); delay(150); ESP.restart(); return; }
  srv.send(200, "text/plain", "ok");
}

void handleMode() {
  uint8_t m = (uint8_t)clampi(srv.arg("m").toInt(), 0, MODE_COUNT - 1);
  P.mode = m;
  Brain::applyMode();
  srv.send(200, "text/plain", "ok");
}

void handleDrive() {
  int16_t l = clampi(srv.arg("l").toInt(), -1000, 1000);
  int16_t r = clampi(srv.arg("r").toInt(), -1000, 1000);
  if (l == 0 && r == 0) { Mot::applyNow(0, 0, true); Brain::go(ST_MANUAL); }
  else Brain::manual(l, r);
  srv.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  OTA
// ---------------------------------------------------------------------
void handleOtaEnd() {
  srv.sendHeader("Connection", "close");
  srv.send(200, "text/plain",
           otaOk ? "gravado! reiniciando em 1s..." : "FALHOU - binario invalido");
  delay(400);
  if (otaOk) ESP.restart();
}

void handleOtaUpload() {
  HTTPUpload& up = srv.upload();
  if (up.status == UPLOAD_FILE_START) {
    Brain::disarm();
    otaOk = false;
    Face::say("GRAVANDO OTA");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    otaOk = Update.end(true);
    if (!otaOk) Update.printError(Serial);
  }
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
  Serial.printf("[web] softAP %s | IP %s | MAC %s\n",
                apOk ? "OK" : "FALHOU", ipStr, WiFi.softAPmacAddress().c_str());

  dns.start(53, "*", WiFi.softAPIP());       // portal cativo: qualquer URL cai no painel
  if (MDNS.begin("robosumo")) MDNS.addService("http", "tcp", 80);

  srv.on("/", HTTP_GET, [] {
    srv.sendHeader("Cache-Control", "no-store");
    srv.send_P(200, "text/html", PAGE_HTML);
  });
  srv.on("/api/state",  HTTP_GET, sendState);
  srv.on("/api/params", HTTP_GET, sendParams);
  srv.on("/api/log",    HTTP_GET, sendLog);
  srv.on("/api/selftest", HTTP_GET, sendSelfTest);
  srv.on("/api/set",    HTTP_GET, handleSet);
  srv.on("/api/cmd",    HTTP_GET, handleCmd);
  srv.on("/api/mode",   HTTP_GET, handleMode);
  srv.on("/api/drive",  HTTP_GET, handleDrive);
  srv.on("/update",     HTTP_POST, handleOtaEnd, handleOtaUpload);
  srv.onNotFound([] {                        // redireciona tudo para o painel
    srv.sendHeader("Location", String("http://") + ipStr + "/", true);
    srv.send(302, "text/plain", "");
  });
  srv.begin();
}

// Task dedicada no core 0: HTTP nunca atrapalha a malha de controle
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
    dns.processNextRequest();
    srv.handleClient();

    // Heartbeat de rede: mostra se algum celular chegou a se conectar no
    // AP, mesmo que o painel HTTP em si nao esteja respondendo direito.
    if (millis() - dbgT > 3000) {
      dbgT = millis();
      uint8_t n = WiFi.softAPgetStationNum();
      T.apClients = n;
      if (n != lastN) {
        Serial.printf("[web] estacoes conectadas ao AP: %u\n", n);
        lastN = n;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

} // namespace Web
