// =====================================================================
//  ROBO SUMO - page.h
//  Painel de controle servido pela propria ESP32 (sem internet, sem CDN).
// =====================================================================
#pragma once
#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="pt-BR"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ROBO SUMO :: painel</title>
<style>
:root{
  --bg:#0a0d14; --panel:#121722; --panel2:#0e131d; --line:#1f2836;
  --tx:#e6edf7; --dim:#7d8ba3; --acc:#00e5a0; --acc2:#37b6ff;
  --hot:#ff4d6d; --warn:#ffb43a; --evil:#c964ff;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:14px/1.45 ui-monospace,"Cascadia Mono",Consolas,monospace;
     padding:10px;max-width:1180px;margin:0 auto;-webkit-tap-highlight-color:transparent}
h2{font-size:12px;letter-spacing:.18em;color:var(--dim);text-transform:uppercase;margin-bottom:10px;font-weight:600}
.grid{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px}
.card.wide{grid-column:1/-1}

/* ---------- cabecalho ---------- */
header{background:linear-gradient(135deg,#121a28,#0d1119);border:1px solid var(--line);
       border-radius:14px;padding:14px;margin-bottom:10px;display:flex;flex-wrap:wrap;
       gap:12px;align-items:center;justify-content:space-between}
.brand{font-size:19px;font-weight:700;letter-spacing:.14em}
.brand span{color:var(--acc)}
.pill{padding:5px 12px;border-radius:999px;font-size:12px;font-weight:700;letter-spacing:.1em;
      border:1px solid var(--line);background:var(--panel2)}
.pill.on{background:rgba(0,229,160,.14);color:var(--acc);border-color:rgba(0,229,160,.4)}
.pill.hot{background:rgba(255,77,109,.14);color:var(--hot);border-color:rgba(255,77,109,.45)}
.pill.warn{background:rgba(255,180,58,.14);color:var(--warn);border-color:rgba(255,180,58,.45)}
.hb{width:9px;height:9px;border-radius:50%;background:var(--hot);display:inline-block;margin-right:6px}
.hb.alive{background:var(--acc);box-shadow:0 0 8px var(--acc)}

/* ---------- botoes ---------- */
button{font:inherit;font-weight:700;cursor:pointer;border-radius:10px;border:1px solid var(--line);
       background:var(--panel2);color:var(--tx);padding:10px 14px;transition:.12s}
button:active{transform:translateY(1px)}
button:hover{border-color:var(--acc2)}
.big{padding:16px 20px;font-size:16px;letter-spacing:.1em;flex:1}
.go{background:linear-gradient(135deg,#00e5a0,#00b884);color:#04120c;border:none}
.stop{background:linear-gradient(135deg,#ff4d6d,#c9203f);color:#fff;border:none}
.row{display:flex;gap:8px;flex-wrap:wrap}

/* ---------- modos ---------- */
.modes{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.mode{text-align:left;padding:11px;line-height:1.3}
.mode b{display:block;font-size:13px;letter-spacing:.08em}
.mode i{font-style:normal;color:var(--dim);font-size:11px}
.mode.sel{border-color:var(--acc);background:rgba(0,229,160,.09)}
.mode.sel b{color:var(--acc)}
.mode[data-m="2"].sel{border-color:var(--evil);background:rgba(201,100,255,.1)}
.mode[data-m="2"].sel b{color:var(--evil)}

/* ---------- leituras ---------- */
.kv{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px dashed var(--line);font-size:13px}
.kv:last-child{border:0}
.kv b{color:var(--acc2);font-weight:700}
.bar{height:9px;background:var(--panel2);border-radius:5px;overflow:hidden;border:1px solid var(--line)}
.bar>i{display:block;height:100%;background:linear-gradient(90deg,var(--acc),var(--acc2));width:0;transition:width .1s}
.ir{display:flex;gap:8px}
.ir>div{flex:1;text-align:center;padding:12px 6px;border-radius:10px;border:1px solid var(--line);
        background:var(--panel2);font-size:12px;font-weight:700;letter-spacing:.08em}
.ir>div.hit{background:rgba(255,77,109,.22);border-color:var(--hot);color:#fff;
            box-shadow:0 0 16px rgba(255,77,109,.45)}
canvas{width:100%;display:block;border-radius:10px;background:var(--panel2);border:1px solid var(--line)}

/* ---------- sliders ---------- */
.tune label{display:block;margin:9px 0 2px;font-size:11px;color:var(--dim);letter-spacing:.06em;
            display:flex;justify-content:space-between}
.tune label span{color:var(--acc);font-weight:700}
input[type=range]{width:100%;height:22px;-webkit-appearance:none;background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:var(--line);border-radius:2px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;border-radius:50%;
   background:var(--acc);margin-top:-6px;border:2px solid var(--bg)}
input[type=range]::-moz-range-track{height:4px;background:var(--line);border-radius:2px}
input[type=range]::-moz-range-thumb{width:14px;height:14px;border:2px solid var(--bg);border-radius:50%;background:var(--acc)}
.chk{display:flex;align-items:center;gap:7px;margin:7px 0;font-size:12px;color:var(--dim)}

/* ---------- dpad ---------- */
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;max-width:230px;margin:0 auto}
.pad button{padding:16px 0;font-size:16px}
.pad .sp{visibility:hidden}

/* ---------- log ---------- */
table{width:100%;border-collapse:collapse;font-size:12px}
th{text-align:left;color:var(--dim);font-weight:600;padding:5px;border-bottom:1px solid var(--line);
   font-size:10px;letter-spacing:.1em}
td{padding:5px;border-bottom:1px solid var(--panel2)}
.tag{padding:2px 7px;border-radius:5px;font-size:10px;font-weight:700;letter-spacing:.06em}
.t0{background:#1b2433;color:var(--dim)}      .t1{background:rgba(55,182,255,.2);color:var(--acc2)}
.t2{background:rgba(0,229,160,.18);color:var(--acc)} .t3{background:rgba(255,77,109,.2);color:var(--hot)}
.t4{background:rgba(255,180,58,.18);color:var(--warn)} .t5{background:rgba(201,100,255,.2);color:var(--evil)}
.t6{background:rgba(0,229,160,.3);color:#fff}  .t7{background:rgba(255,77,109,.35);color:#fff}
.foot{color:var(--dim);font-size:11px;text-align:center;padding:16px 0 4px}
.note{color:var(--dim);font-size:11px;margin-top:8px;line-height:1.5}
progress{width:100%;height:8px}
</style></head><body>

<header>
  <div>
    <div class="brand">ROBO<span>SUMO</span></div>
    <div style="font-size:11px;color:var(--dim);margin-top:3px">
      <span class="hb" id="hb"></span><span id="ip">painel local</span> &middot; <span id="up">--</span>
    </div>
  </div>
  <div class="row">
    <span class="pill" id="pMode">--</span>
    <span class="pill" id="pState">--</span>
    <span class="pill" id="pBat">--</span>
  </div>
</header>

<div class="row" style="margin-bottom:10px">
  <button class="big go"   onclick="cmd('arm')">&#9654; ARMAR (5s)</button>
  <button class="big stop" onclick="cmd('stop')">&#9632; PARAR</button>
  <button class="big"      onclick="cmd('dance')">&#9834; DANCINHA</button>
</div>

<div class="grid">

  <!-- ============ MODOS ============ -->
  <div class="card wide">
    <h2>Modo de combate</h2>
    <div class="modes" id="modes"></div>
  </div>

  <!-- ============ VISAO ============ -->
  <div class="card">
    <h2>Visao / radar</h2>
    <canvas id="radar" width="440" height="260"></canvas>
    <div class="kv"><span>Ultrassom esquerdo</span><b id="dl">--</b></div>
    <div class="bar" style="margin-bottom:6px"><i id="bl"></i></div>
    <div class="kv"><span>Ultrassom direito</span><b id="dr">--</b></div>
    <div class="bar" style="margin-bottom:10px"><i id="br"></i></div>
    <div class="kv"><span>Confianca do alvo</span><b id="cf">0%</b></div>
    <div class="bar"><i id="bc"></i></div>
  </div>

  <!-- ============ BORDA ============ -->
  <div class="card">
    <h2>Sensores de borda (salva-vidas)</h2>
    <div class="ir">
      <div id="irL">IR ESQ<br><span id="irLv" style="color:var(--dim)">--</span></div>
      <div id="irR">IR DIR<br><span id="irRv" style="color:var(--dim)">--</span></div>
    </div>
    <div class="kv" style="margin-top:8px"><span>Pino digital DO (esq / dir)</span><b id="irdo">-- / --</b></div>
    <div class="kv"><span>Olhos VL53L0X no barramento</span><b id="tofok">-- / --</b></div>
    <div class="kv"><span>Leitura crua em mm (esq / dir)</span><b id="tofraw">-- / --</b></div>
    <div class="kv"><span>Leituras invalidas seguidas</span><b id="toffail">-- / --</b></div>
    <div class="note">Zona segura = escuro (valor ADC alto). Borda = claro (valor baixo).
      Ajuste o <b>limiar</b> abaixo entre os dois valores lidos: coloque o robo no centro, anote;
      coloque em cima da faixa branca, anote; use a media.</div>
    <label style="display:flex;justify-content:space-between;font-size:11px;color:var(--dim);margin-top:10px">
      LIMIAR IR (ADC) <span id="vIr" style="color:var(--acc);font-weight:700"></span></label>
    <input type="range" id="irThreshold" min="200" max="3800" step="25">
    <label style="display:flex;justify-content:space-between;font-size:11px;color:var(--dim);margin-top:10px">FONTE DO SINAL DE BORDA</label><select id="irSource" style="width:100%;padding:8px;border-radius:8px;background:var(--panel2);color:var(--tx);border:1px solid var(--line);font:inherit"><option value="0">Somente pino DO (digital)</option><option value="1">Somente AO (ADC) &mdash; recomendado</option><option value="2">Qualquer um dos dois</option></select>
    <div class="chk"><input type="checkbox" id="irActiveLow"><label for="irActiveLow">Sensor indica borda com nivel BAIXO</label></div>
  </div>

  <!-- ============ MOTORES / PID ============ -->
  <div class="card">
    <h2>Saida dos motores &amp; PID</h2>
    <canvas id="chart" width="440" height="180"></canvas>
    <div class="kv"><span>PWM esquerdo</span><b id="pl">0</b></div>
    <div class="kv"><span>PWM direito</span><b id="pr">0</b></div>
    <div class="kv"><span>PID  P / I / D</span><b id="pid">0 / 0 / 0</b></div>
    <div class="kv"><span>Saida do PID</span><b id="po">0</b></div>
    <div class="kv"><span>Malha de controle</span><b id="hz">-- Hz</b></div>
  </div>

  <!-- ============ DESEMPENHO ============ -->
  <div class="card">
    <h2>Desempenho da luta</h2>
    <div class="kv"><span>Investidas iniciadas</span><b id="na">0</b></div>
    <div class="kv"><span>Salvamentos na borda</span><b id="ne">0</b></div>
    <div class="kv"><span>Alvos perdidos</span><b id="nl">0</b></div>
    <div class="kv"><span>Tempo em ataque</span><b id="ta">0 s</b></div>
    <div class="kv"><span>Aproveitamento</span><b id="ef">--</b></div>
    <div class="kv"><span>Falha driver (nFAULT)</span><b id="fl">nao</b></div>
    <div class="kv"><span>Bateria</span><b id="vb">--</b></div>
  </div>

  <!-- ============ AJUSTES ============ -->
  <div class="card tune">
    <h2>Afinacao (base &mdash; o modo multiplica em cima disso)</h2>
    <label>VELOCIDADE DE BUSCA <span id="vvSearch"></span></label>
    <input type="range" id="vSearch" min="150" max="1000" step="10">
    <label>VELOCIDADE DE INVESTIDA <span id="vvAttack"></span></label>
    <input type="range" id="vAttack" min="200" max="1000" step="10">
    <label>TETO DE VELOCIDADE (protege motor 6V) <span id="vvMax"></span></label>
    <input type="range" id="vMax" min="300" max="1000" step="10">
    <label>VELOCIDADE DE RECUO <span id="vvReverse"></span></label>
    <input type="range" id="vReverse" min="300" max="1000" step="10">
    <label>KP <span id="vkp"></span></label><input type="range" id="kp" min="0" max="200" step="1">
    <label>KI <span id="vki"></span></label><input type="range" id="ki" min="0" max="100" step="1">
    <label>KD <span id="vkd"></span></label><input type="range" id="kd" min="0" max="200" step="1">
    <label>ALCANCE DE DETECCAO (cm) <span id="vrangeCm"></span></label>
    <input type="range" id="rangeCm" min="20" max="130" step="1">
    <label>LEITURAS P/ CONFIRMAR ALVO <span id="vconfirmHits"></span></label>
    <input type="range" id="confirmHits" min="1" max="10" step="1">
    <label>SALTO MAXIMO ENTRE LEITURAS (cm) <span id="vjumpCm"></span></label>
    <input type="range" id="jumpCm" min="5" max="90" step="1">
    <label>LEITURAS VAZIAS P/ PERDER ALVO <span id="vloseMisses"></span></label>
    <input type="range" id="loseMisses" min="2" max="20" step="1">
  </div>

  <div class="card tune">
    <h2>Manobras</h2>
    <label>RECUO NA BORDA (ms) <span id="vedgeBackMs"></span></label>
    <input type="range" id="edgeBackMs" min="80" max="700" step="10">
    <label>GIRO APOS O RECUO (ms) <span id="vedgeTurnMs"></span></label>
    <input type="range" id="edgeTurnMs" min="60" max="800" step="10">
    <label>DURACAO DA VARREDURA (ms) <span id="vsweepMs"></span></label>
    <input type="range" id="sweepMs" min="200" max="2500" step="25">
    <label>RAMPA DE ACELERACAO (ms) <span id="vrampMs"></span></label>
    <input type="range" id="rampMs" min="0" max="600" step="10">
    <label>TEMPO SEM PROGRESSO P/ DESTRAVAR (ms) <span id="vstuckMs"></span></label>
    <input type="range" id="stuckMs" min="400" max="4000" step="50">
    <label>CONTAGEM REGRESSIVA (ms) <span id="vcountdownMs"></span></label>
    <input type="range" id="countdownMs" min="0" max="8000" step="250">
    <label>CORTE POR BATERIA BAIXA (centesimos de V) <span id="vvbatMin"></span></label>
    <input type="range" id="vbatMin" min="500" max="800" step="5">
    <div class="chk"><input type="checkbox" id="soundOn"><label for="soundOn">Buzzer ligado</label></div>
    <div class="chk"><input type="checkbox" id="faceOn"><label for="faceOn">Carinha no OLED</label></div>
    <div class="chk"><input type="checkbox" id="autoRestart"><label for="autoRestart">Dancar ao vencer e voltar pra luta</label></div>
    <div class="row" style="margin-top:12px">
      <button onclick="cmd('save')" style="flex:1">SALVAR NA MEMORIA</button>
      <button onclick="cmd('reset')">PADRAO</button>
    </div>
    <div class="note">Os sliders valem na hora. &ldquo;Salvar&rdquo; grava na NVS e sobrevive ao desligamento.</div>
  </div>

  <!-- ============ MANUAL ============ -->
  <div class="card">
    <h2>Pilotagem manual / teste de bancada</h2>
    <div class="pad">
      <button class="sp"></button>
      <button onpointerdown="drv(700,700)"   onpointerup="drv(0,0)" onpointerleave="drv(0,0)">&#9650;</button>
      <button class="sp"></button>
      <button onpointerdown="drv(-600,600)"  onpointerup="drv(0,0)" onpointerleave="drv(0,0)">&#9664;</button>
      <button onpointerdown="drv(0,0)">&#9632;</button>
      <button onpointerdown="drv(600,-600)"  onpointerup="drv(0,0)" onpointerleave="drv(0,0)">&#9654;</button>
      <button class="sp"></button>
      <button onpointerdown="drv(-700,-700)" onpointerup="drv(0,0)" onpointerleave="drv(0,0)">&#9660;</button>
      <button class="sp"></button>
    </div>
    <div class="row" style="margin-top:12px;justify-content:center">
      <button onclick="cmd('testL')">TESTAR ESQ</button>
      <button onclick="cmd('testR')">TESTAR DIR</button>
      <button onclick="cmd('beep')">TESTAR BUZZER</button>
    </div>
    <div class="chk"><input type="checkbox" id="motInvL"><label for="motInvL">Inverter sentido do lado ESQUERDO</label></div>
    <div class="chk"><input type="checkbox" id="motInvR"><label for="motInvR">Inverter sentido do lado DIREITO</label></div>
    <div class="note">Serve para conferir se cada lado gira no sentido certo. Se um lado estiver
      invertido, basta trocar os dois fios daquele motor no borne da DRV8833.</div>
  </div>

  <!-- ============ LOG ============ -->
  <div class="card wide">
    <h2>Registro de combate</h2>
    <table><thead><tr><th>T+</th><th>EVENTO</th><th>DADO A</th><th>DADO B</th></tr></thead>
    <tbody id="log"><tr><td colspan="4" style="color:var(--dim)">sem eventos ainda</td></tr></tbody></table>
  </div>

  <!-- ============ OTA ============ -->
  <div class="card wide">
    <h2>Gravar firmware sem PC (OTA)</h2>
    <div class="note">Exporte o binario na Arduino IDE (<b>Sketch &rarr; Export Compiled Binary</b>),
      mande o arquivo <b>.bin</b> por aqui de qualquer celular ligado no Wi-Fi do robo. Depois do
      envio a ESP reinicia sozinha. Nao envie com o robo armado.</div>
    <form id="ota" method="POST" action="/update" enctype="multipart/form-data" style="margin-top:10px">
      <input type="file" name="fw" accept=".bin" style="margin-bottom:8px">
      <button type="submit">ENVIAR FIRMWARE</button>
    </form>
    <progress id="otap" value="0" max="100" style="display:none;margin-top:8px"></progress>
    <div id="otam" class="note"></div>
  </div>
</div>

<div class="foot">ESP32 &middot; firmware <span id="fw">--</span> &middot; painel servido pelo proprio robo &middot; sem senha, sem nuvem</div>

<script>
const $=id=>document.getElementById(id);
const MODES=[
 [0,"NORMAL","equilibrio entre busca e forca"],
 [1,"LESMA","lento e paciente, nao se afoba"],
 [2,"CAPIROTO","extremo: tudo no maximo"],
 [3,"CACADOR","alcance longo, varre a arena"],
 [4,"MURALHA","fica no centro e contra-ataca"],
 [5,"DANCINHA","so a coreografia"]];
let P={},curMode=0,hist=[],failed=0;

/* ---------- modos ---------- */
$("modes").innerHTML=MODES.map(m=>
 `<button class="mode" data-m="${m[0]}" onclick="setMode(${m[0]})"><b>${m[1]}</b><i>${m[2]}</i></button>`).join("");
function setMode(m){curMode=m;paintModes();fetch("/api/mode?m="+m);}
function paintModes(){document.querySelectorAll(".mode").forEach(b=>
  b.classList.toggle("sel",+b.dataset.m===curMode));}

/* ---------- comandos ---------- */
function cmd(c){fetch("/api/cmd?c="+c).then(()=>{if(c=="reset")setTimeout(loadParams,250);});}
/* o robo para sozinho se ficar 700 ms sem comando, entao enquanto o
   botao estiver pressionado a gente reenvia o mesmo comando */
let drvT=null;
function drv(l,r){
 clearInterval(drvT);drvT=null;
 fetch("/api/drive?l="+l+"&r="+r);
 if(l||r)drvT=setInterval(()=>fetch("/api/drive?l="+l+"&r="+r),250);
}
addEventListener("pointerup",()=>{if(drvT)drv(0,0);});
addEventListener("blur",()=>{if(drvT)drv(0,0);});

/* ---------- parametros ---------- */
const INT=["vSearch","vAttack","vMax","vReverse","rangeCm","confirmHits","jumpCm","loseMisses",
  "edgeBackMs","edgeTurnMs","sweepMs","rampMs","stuckMs","countdownMs","irThreshold","vbatMin"];
const FLT={kp:10,ki:100,kd:10};            /* slider = valor * fator */
const BOOL=["soundOn","faceOn","autoRestart","irActiveLow","motInvL","motInvR"];
const SEL=["irSource"];

function loadParams(){fetch("/api/params").then(r=>r.json()).then(p=>{
  P=p;curMode=p.mode;paintModes();
  INT.forEach(k=>{const e=$(k);if(!e)return;e.value=p[k];lbl(k,p[k]);});
  for(const k in FLT){const e=$(k);if(!e)continue;e.value=Math.round(p[k]*FLT[k]);lbl(k,p[k]);}
  BOOL.forEach(k=>{const e=$(k);if(e)e.checked=!!p[k];});
  SEL.forEach(k=>{const e=$(k);if(e)e.value=p[k];});
});}
function lbl(k,v){const e=$("v"+k)||$("vv"+k);if(e)e.textContent=v;}

let sendT=null,pend={};
function push(k,v){pend[k]=v;clearTimeout(sendT);sendT=setTimeout(()=>{
  const q=Object.entries(pend).map(([a,b])=>a+"="+b).join("&");pend={};
  fetch("/api/set?"+q);},90);}

INT.forEach(k=>{const e=$(k);if(e)e.oninput=()=>{lbl(k,e.value);push(k,e.value);};});
for(const k in FLT){const e=$(k);if(e)e.oninput=()=>{const v=(e.value/FLT[k]).toFixed(2);lbl(k,v);push(k,v);};}
BOOL.forEach(k=>{const e=$(k);if(e)e.onchange=()=>push(k,e.checked?1:0);});
SEL.forEach(k=>{const e=$(k);if(e)e.onchange=()=>push(k,e.value);});

/* ---------- telemetria ---------- */
function poll(){
 fetch("/api/state").then(r=>r.json()).then(s=>{
  failed=0;$("hb").className="hb alive";
  $("pMode").textContent=s.modeName;
  $("pState").textContent=s.stateName+(s.state==1?" "+Math.ceil(s.cd/1000):"");
  $("pState").className="pill "+(s.state==5?"hot":s.state==4?"warn":s.armed?"on":"");
  $("pMode").className="pill "+(s.mode==2?"hot":"on");
  $("pBat").textContent=(s.vbat/100).toFixed(2)+" V";
  $("pBat").className="pill "+(s.vbat<P.vbatMin+20?"hot":"");
  $("up").textContent="ligado ha "+fmt(s.up);
  if(s.fw)$("fw").textContent="v"+s.fw;

  const R=P.rangeCm||70;
  $("dl").textContent=s.dl<0?"---":s.dl+" cm";
  $("dr").textContent=s.dr<0?"---":s.dr+" cm";
  $("bl").style.width=(s.dl<0?0:100-Math.min(100,s.dl*100/R))+"%";
  $("br").style.width=(s.dr<0?0:100-Math.min(100,s.dr*100/R))+"%";
  $("cf").textContent=s.cf+"%";$("bc").style.width=s.cf+"%";

  $("irL").className=s.irL?"hit":"";$("irR").className=s.irR?"hit":"";
  $("irLv").textContent=s.irLr;$("irRv").textContent=s.irRr;
  $("irdo").textContent=(s.irLd?"BORDA":"seguro")+" / "+(s.irRd?"BORDA":"seguro");
  $("tofok").textContent=(s.tofOkL?"OK":"AUSENTE")+" / "+(s.tofOkR?"OK":"AUSENTE");
  $("tofraw").textContent=(s.tofL<0?"sem alvo":s.tofL+" mm")+" / "+(s.tofR<0?"sem alvo":s.tofR+" mm");
  $("toffail").textContent=s.tofFL+" / "+s.tofFR;

  $("pl").textContent=s.pl;$("pr").textContent=s.pr;
  $("pid").textContent=s.pp.toFixed(1)+" / "+s.pi.toFixed(1)+" / "+s.pd.toFixed(1);
  $("po").textContent=s.po.toFixed(0);
  $("hz").textContent=s.hz+" Hz";
  $("na").textContent=s.na;$("ne").textContent=s.ne;$("nl").textContent=s.nl;
  $("ta").textContent=(s.ta/1000).toFixed(1)+" s";
  $("ef").textContent=s.na?Math.round(100*(s.na-s.nl)/s.na)+"% das investidas mantidas":"--";
  $("fl").textContent=s.flt?"SIM":"nao";
  $("vb").textContent=(s.vbat/100).toFixed(2)+" V";

  hist.push([s.pl,s.pr,s.po]);if(hist.length>180)hist.shift();
  radar(s);chart();
 }).catch(()=>{if(++failed>3)$("hb").className="hb";});
}
function fmt(ms){const s=Math.floor(ms/1000);return (s>=60?Math.floor(s/60)+"m ":"")+(s%60)+"s";}

/* ---------- radar ---------- */
function radar(s){
 const c=$("radar"),x=c.getContext("2d"),W=c.width,H=c.height;
 x.clearRect(0,0,W,H);
 const cx=W/2,cy=H-24,R=P.rangeCm||70,SC=(H-44)/R;
 x.strokeStyle="#1f2836";x.lineWidth=1;
 for(let d=10;d<=R;d+=10){x.beginPath();x.arc(cx,cy,d*SC,Math.PI,0);x.stroke();}
 x.fillStyle="#5b6a83";x.font="10px monospace";
 for(let d=20;d<=R;d+=20)x.fillText(d+"cm",cx+4,cy-d*SC-3);
 /* cones dos dois sensores (aprox. 15 graus cada, divergentes) */
 [[-1,s.dl],[1,s.dr]].forEach(([sd,d])=>{
   const a=-Math.PI/2+sd*0.28;
   x.strokeStyle=d<0?"#233047":"rgba(0,229,160,.55)";
   x.beginPath();x.moveTo(cx,cy);
   x.arc(cx,cy,(d<0?R:d)*SC,a-0.22,a+0.22);x.closePath();x.stroke();
   if(d>=0){x.fillStyle="rgba(0,229,160,.13)";x.fill();
     x.fillStyle="#00e5a0";x.beginPath();
     x.arc(cx+Math.cos(a)*d*SC,cy+Math.sin(a)*d*SC,5,0,7);x.fill();}
 });
 /* o robo */
 x.fillStyle=s.state==4?"#ff4d6d":"#37b6ff";
 x.beginPath();x.moveTo(cx,cy-12);x.lineTo(cx-11,cy+8);x.lineTo(cx+11,cy+8);x.closePath();x.fill();
 /* vetor de direcao do PID */
 if(s.armed){x.strokeStyle="#ffb43a";x.lineWidth=2;x.beginPath();x.moveTo(cx,cy);
   x.lineTo(cx+s.po*0.09,cy-34);x.stroke();}
}

/* ---------- grafico ---------- */
function chart(){
 const c=$("chart"),x=c.getContext("2d"),W=c.width,H=c.height;
 x.clearRect(0,0,W,H);
 x.strokeStyle="#1f2836";x.beginPath();x.moveTo(0,H/2);x.lineTo(W,H/2);x.stroke();
 const cols=["#00e5a0","#37b6ff","#ffb43a"];
 for(let k=0;k<3;k++){
  x.strokeStyle=cols[k];x.lineWidth=k==2?1:1.6;x.beginPath();
  hist.forEach((v,i)=>{const px=i*W/180,py=H/2-v[k]*(H/2-6)/1000;
   i?x.lineTo(px,py):x.moveTo(px,py);});
  x.stroke();
 }
 x.fillStyle="#5b6a83";x.font="10px monospace";
 x.fillText("verde=PWM esq   azul=PWM dir   ambar=PID",6,12);
}

/* ---------- log ---------- */
const TAGS=["ARM","LOCK","ATAQUE","BORDA","PERDEU","TRAVOU","VITORIA","FALHA"];
function loadLog(){fetch("/api/log").then(r=>r.json()).then(v=>{
 if(!v.length){$("log").innerHTML='<tr><td colspan="4" style="color:var(--dim)">sem eventos ainda</td></tr>';return;}
 $("log").innerHTML=v.slice().reverse().map(e=>
  `<tr><td>${(e.t/1000).toFixed(2)}s</td><td><span class="tag t${e.y}">${TAGS[e.y]}</span></td>
   <td>${e.a}</td><td>${e.b}</td></tr>`).join("");
});}

/* ---------- OTA ---------- */
$("ota").onsubmit=ev=>{ev.preventDefault();
 const f=$("ota").querySelector("input[type=file]").files[0];
 if(!f){$("otam").textContent="escolha um arquivo .bin";return;}
 const fd=new FormData();fd.append("fw",f,f.name);
 const xhr=new XMLHttpRequest();xhr.open("POST","/update");
 $("otap").style.display="block";
 xhr.upload.onprogress=e=>{$("otap").value=e.loaded*100/e.total;
   $("otam").textContent="enviando "+Math.round(e.loaded*100/e.total)+"%";};
 xhr.onload=()=>{$("otam").textContent=xhr.responseText||"pronto, reiniciando...";};
 xhr.onerror=()=>{$("otam").textContent="falhou o envio";};
 xhr.send(fd);};

loadParams();poll();loadLog();
setInterval(poll,130);
setInterval(loadLog,1500);
</script></body></html>)HTMLPAGE";
