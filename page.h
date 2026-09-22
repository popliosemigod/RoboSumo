// =====================================================================
//  ROBO SUMO v4 - page.h
//  Painel minimo: leitura dos sensores, start/stop, modos e pilotagem
//  manual das duas rodas.
//
//  O que NAO tem aqui e ajuste de parametro. Slider no celular parece
//  controle e nao e: o numero que funcionou nao fica versionado nem
//  comentado, e na sessao seguinte ninguem sabe se o valor em uso e o do
//  codigo ou o que sobrou na NVS.
// =====================================================================
#pragma once

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Robo Sumo</title>
<style>
:root{--bg:#12141a;--card:#1b1e26;--line:#2c313d;--tx:#e7e9ee;--dim:#949bad;
      --ok:#3ddc84;--bad:#ff5b5b;--acc:#ffb020}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
     padding:14px;max-width:560px;margin:0 auto;-webkit-tap-highlight-color:transparent}
h1{font-size:17px;margin:0 0 12px;letter-spacing:.06em;color:var(--dim);font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.09em;color:var(--dim);margin:0 0 10px;font-weight:600}
.kv{display:flex;justify-content:space-between;align-items:baseline;padding:6px 0;border-bottom:1px solid var(--line)}
.kv:last-child{border-bottom:0}
.kv span{color:var(--dim);font-size:13px}
.kv b{font-variant-numeric:tabular-nums;font-size:15px}
.big{font-size:30px;font-weight:700;font-variant-numeric:tabular-nums;text-align:center;padding:6px 0}
.st{text-align:center;font-size:19px;font-weight:700;letter-spacing:.04em;padding:4px 0 10px}
.pill{display:inline-block;padding:2px 9px;border-radius:99px;font-size:12px;font-weight:600}
.on{background:rgba(61,220,132,.16);color:var(--ok)}
.al{background:rgba(255,91,91,.16);color:var(--bad)}
button{font:inherit;border:0;border-radius:12px;cursor:pointer;color:#12141a;background:var(--ok);
       padding:16px;font-size:17px;font-weight:700;width:100%;letter-spacing:.03em}
button.stop{background:var(--bad);color:#fff}
.row{display:flex;gap:10px;margin-top:10px}
.row button{font-size:14px;padding:12px;background:var(--line);color:var(--tx);font-weight:600}
/* teclado de direcao: duas rodas so permitem cinco comandos */
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;justify-items:stretch}
.pad button{background:var(--line);color:var(--tx);font-size:22px;padding:18px 0;font-weight:700}
.pad button:active{background:var(--acc);color:#12141a}
.pad .sp{visibility:hidden}
.pad .halt{background:rgba(255,91,91,.18);color:var(--bad)}
.modes{display:flex;flex-wrap:wrap;gap:8px}
.modes button{width:auto;flex:1 1 30%;padding:10px 6px;font-size:13px;background:var(--line);color:var(--tx);font-weight:600}
.modes button.sel{background:var(--acc);color:#12141a}
.foot{color:var(--dim);font-size:12px;text-align:center;margin-top:14px}
</style></head><body>

<h1>ROBO SUMO</h1>

<div class="card">
  <div class="st" id="st">--</div>
  <button id="go">START</button>
  <div class="row"><button onclick="cmd('reboot')">Reiniciar</button></div>
</div>

<div class="card">
  <h2>Modo de combate</h2>
  <div class="modes" id="modes"></div>
</div>

<div class="card">
  <h2>Pilotagem manual</h2>
  <div class="pad">
    <span class="sp"></span>
    <button onpointerdown="dir('fwd')"  onpointerup="dir('stop')" onpointercancel="dir('stop')">&#9650;</button>
    <span class="sp"></span>
    <button onpointerdown="dir('left')" onpointerup="dir('stop')" onpointercancel="dir('stop')">&#9664;</button>
    <button class="halt" onpointerdown="dir('stop')">&#9632;</button>
    <button onpointerdown="dir('right')" onpointerup="dir('stop')" onpointercancel="dir('stop')">&#9654;</button>
    <span class="sp"></span>
    <button onpointerdown="dir('back')" onpointerup="dir('stop')" onpointercancel="dir('stop')">&#9660;</button>
    <span class="sp"></span>
  </div>
</div>

<div class="card">
  <h2>Olho &mdash; HC-SR04</h2>
  <div class="big" id="dist">-- cm</div>
  <div class="kv"><span>Eco cru</span><b id="echo">-- us</b></div>
  <div class="kv"><span>Confianca no alvo</span><b id="cf">0%</b></div>
  <div class="kv"><span>Leituras invalidas seguidas</span><b id="fail">0</b></div>
</div>

<div class="card">
  <h2>Borda &mdash; IR analogico</h2>
  <div class="kv"><span>Leitura</span><b id="ir">--</b></div>
  <div class="kv"><span>AO (ja dividido)</span><b id="irmv">-- mV</b></div>
  <div class="kv"><span>Limiar no codigo</span><b id="lim">-- mV</b></div>
  <div class="kv"><span>Salvamentos nesta luta</span><b id="ne">0</b></div>
</div>

<div class="card">
  <h2>Sistema</h2>
  <div class="kv"><span>PWM esquerda / direita</span><b id="pwm">0 / 0</b></div>
  <div class="kv"><span>Ataques / perdas</span><b id="na">0 / 0</b></div>
  <div class="kv"><span>Malha de controle</span><b id="hz">-- Hz</b></div>
  <div class="kv"><span>Ligado ha</span><b id="up">--</b></div>
</div>

<div class="foot">firmware <span id="fw">--</span> &middot; atualiza 5x por segundo</div>

<script>
const $=i=>document.getElementById(i);
const MODOS=['NORMAL','LESMA','CAPIROTO','CACADOR','MURALHA','DANCINHA'];
let modoAtual=-1, dirAtual='stop';

function cmd(c){fetch('/api/cmd?c='+c).then(tick)}
$('go').onclick=()=>cmd($('go').dataset.a||'arm');

/* Botoes de modo, montados uma vez */
MODOS.forEach((n,i)=>{
  const b=document.createElement('button');
  b.textContent=n; b.id='md'+i;
  b.onclick=()=>fetch('/api/mode?m='+i).then(tick);
  $('modes').appendChild(b);
});

/* Pilotagem: o comando e REENVIADO a cada 300 ms enquanto o botao esta
   pressionado. Isso mantem vivo o corte por silencio do firmware - se a
   aba fechar ou o Wi-Fi cair, o robo para sozinho em 700 ms. */
function dir(d){ dirAtual=d; fetch('/api/drive?d='+d); }
setInterval(()=>{ if(dirAtual!=='stop') fetch('/api/drive?d='+dirAtual); },300);

function pill(el,txt,cls){el.innerHTML='<span class="pill '+cls+'">'+txt+'</span>'}

function tick(){
 fetch('/api/state').then(r=>r.json()).then(d=>{
  $('st').textContent=d.stateName;
  $('st').style.color=d.armed?'var(--ok)':'var(--dim)';

  const b=$('go');
  if(d.armed){b.textContent='STOP';b.className='stop';b.dataset.a='stop'}
  else       {b.textContent='START';b.className='';b.dataset.a='arm'}

  if(d.mode!==modoAtual){
    if(modoAtual>=0) $('md'+modoAtual).classList.remove('sel');
    modoAtual=d.mode; $('md'+modoAtual).classList.add('sel');
  }

  $('dist').textContent = d.d>0 ? d.d+' cm' : 'sem alvo';
  $('dist').style.color = d.d>0 ? 'var(--acc)' : 'var(--dim)';
  $('echo').textContent = d.us ? d.us+' us' : '0 us (sem eco)';
  $('cf').textContent   = d.cf+'%';
  $('fail').textContent = d.fail;

  pill($('ir'), d.ir?'VENDO BORDA':'seguro', d.ir?'al':'on');
  $('irmv').textContent = d.irmv+' mV';
  $('lim').textContent  = d.lim+' mV';
  $('ne').textContent   = d.ne;

  $('pwm').textContent = d.pl+' / '+d.pr;
  $('na').textContent  = d.na+' / '+d.nl;
  $('hz').textContent  = d.hz+' Hz';
  const s=Math.floor(d.up/1000);
  $('up').textContent  = Math.floor(s/60)+'m '+(s%60)+'s';
  $('fw').textContent  = d.fw;
 }).catch(()=>{$('st').textContent='sem conexao';$('st').style.color='var(--bad)'});
}
tick();setInterval(tick,200);
</script>
</body></html>)HTML";
