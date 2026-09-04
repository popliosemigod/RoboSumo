# ROBO SUMO — ESP32

Robô de sumô de porte pequeno. Conta 5 s, procura o oponente, trava o alvo com
confirmação anti-ruído, investe com PID e é salvo da borda por dois sensores IR
com reação por interrupção. Painel web servido pela própria ESP32, sem senha e
sem internet.

---

## 1. Lista de material

| Qtd | Item | Observação |
|-----|------|-----------|
| 1 | ESP32 DevKit V1 (30 pinos) | qualquer DevKit com os mesmos GPIOs serve |
| 4 | Motor N20 6 V 300 RPM | 2 por lado |
| 2 | Driver DRV8833 (módulo) | 1 por lado, 1 canal por motor |
| 2 | VL53L0X (módulo GY-530 / GY-VL53L0XV2) | olhos laser ToF, divergentes ~15° cada |
| 2 | Sensor IR reflexivo (TCRT5000 / módulo de linha) | com saída **DO** e **AO** |
| 1 | OLED SSD1306 128×64 I2C | endereço 0x3C |
| 1 | Buzzer passivo 5 V | passivo — o firmware gera a frequência |
| 1 | Transistor 2N2222 + resistor 1 k | driver do buzzer (opcional se piezo pequeno) |
| 1 | Regulador buck 5 V 2 A (MP1584 / LM2596) | alimenta ESP32 e sensores |
| 1 | Bateria LiPo 2S 7,4 V ≥ 900 mAh 25C | alimenta os DRV8833 direto |
| 1 | Chave gangorra + botão momentâneo | liga/desliga e ARMAR |
| — | 3× 10 kΩ, 100 kΩ, 47 kΩ | pull-down do `nSLEEP`, pull-up do `nFAULT` e divisor da bateria |
| — | 1× 470 µF 16 V, 4× 100 nF | desacoplamento |

---

## 2. Pinout

Escolhido para cair em blocos **fisicamente consecutivos** no header da DevKit,
o que deixa a fiação limpa e o chicote curto.

### Lateral A do header (`VIN GND D13 D12 D14 D27 D26 D25 D33 D32 D35 D34 VN VP EN`)

| GPIO | Função | Ligado em |
|------|--------|-----------|
| 13 | `nSLEEP` das duas pontes | DRV8833 #1 e #2, pino SLP (+ pull-down 10 k) |
| 12 | — livre (reservado) | *não usar: pino de strapping* |
| 14 | **IN4** — lado direito | DRV8833 #2 · AIN2 **e** BIN2 → OUT4 |
| 27 | **IN3** — lado direito | DRV8833 #2 · AIN1 **e** BIN1 → OUT3 |
| 26 | **IN2** — lado esquerdo | DRV8833 #1 · AIN2 **e** BIN2 → OUT2 |
| 25 | **IN1** — lado esquerdo | DRV8833 #1 · AIN1 **e** BIN1 → OUT1 |
| 33 | IR direito — saída analógica | AO do módulo IR direito |
| 32 | IR esquerdo — saída analógica | AO do módulo IR esquerdo |
| 35 | IR direito — saída digital | DO do módulo IR direito (interrupção) |
| 34 | IR esquerdo — saída digital | DO do módulo IR esquerdo (interrupção) |
| 39 (VN) | Leitura da bateria | divisor 100 k / 47 k na saída da LiPo |
| 36 (VP) | — livre | |

### Lateral B do header (`D23 D22 TX D21 D19 D18 D5 D17 D16 D4 D2 D15`)

| GPIO | Função | Ligado em |
|------|--------|-----------|
| 23 | Botão ARMAR/PARAR | botão para GND (pull-up interno) |
| 22 | I2C SCL | OLED SCL |
| 21 | I2C SDA | OLED SDA |
| 19 | `XSHUT` do olho esquerdo | VL53L0X esquerdo · XSHUT |
| 18 | `XSHUT` do olho direito | VL53L0X direito · XSHUT |
| 17 | I2C SCL dos olhos (`Wire1`) | SCL dos **dois** VL53L0X |
| 16 | I2C SDA dos olhos (`Wire1`) | SDA dos **dois** VL53L0X |
| 4 | Buzzer | base do 2N2222 via 1 k |
| 2 | LED da placa | pisca quando armado |
| 15 | `nFAULT` das duas pontes | dreno aberto + pull-up 10 k para 3V3 |

> **Por que 2 GPIOs movem 2 motores por lado:** dentro de cada DRV8833 os **dois canais
> são paralelados** — `AIN1`+`BIN1` viram `IN1`, `AIN2`+`BIN2` viram `IN2`, e do outro
> lado `AOUT1`+`BOUT1` viram `OUT1` e `AOUT2`+`BOUT2` viram `OUT2`. Os dois motores
> daquele lado penduram nesse mesmo par de saídas.
>
> Paralelar é um modo previsto pelo DRV8833 e **dobra a corrente**: 1,5 A RMS por canal
> viram ≈ 3 A no par. Dois N20 travados juntos passam de 1,5 A e desarmariam a proteção
> se dividissem um canal só. E as duas rodas de um lado recebem literalmente a mesma
> tensão, então nunca discordam.
>
> Se um **lado inteiro** girar ao contrário, marque *Inverter sentido* no painel web em
> vez de trocar quatro fios. Se apenas **um motor do par** girar ao contrário, aí sim
> inverta os dois fios daquele motor — ele está com a polaridade trocada em relação ao irmão.

---

## 3. Alimentação

```
LiPo 2S 7,4 V ──┬── chave ──┬── VM dos dois DRV8833  (+ 470 µF + 100 nF)
                │           └── divisor 100k/47k ── GPIO39 (leitura de bateria)
                └── buck 5 V ──┬── VIN da ESP32
                               └── VCC do buzzer
3V3 da ESP32 ──┬── VCC do OLED
               ├── VIN dos 2 VL53L0X
               ├── VCC dos 2 módulos IR
               └── pull-up de 10 k do nFAULT
GND: um único ponto comum entre bateria, buck, ESP32, drivers e sensores.
```

Os motores são de 6 V e a bateria dá 7,4 V. Quem protege é o parâmetro
**TETO DE VELOCIDADE (`vMax`)** — em 820/1000 o motor vê ≈ 6,1 V médios.
No modo CAPIROTO o teto sobe para 1000; use só em lutas curtas.

Os VL53L0X ficam no **3V3 da própria ESP32**: o módulo tem regulador e pull-ups
a bordo e o I2C dele já sai em 3,3 V, então aqui não existe divisor de tensão
nenhum. Dois olhos consomem ≈ 40 mA — folgado para o regulador da DevKit.

Os módulos IR também vão em **3V3**, e isso não é preferência: o `DO` é a saída
do comparador LM393 com pull-up para o **VCC do próprio módulo**, e o `AO` é um
divisor que também chega perto do VCC. Com o módulo em 5 V, esses dois pinos
entregam **5 V** nos GPIOs 34/35 e 32/33 — e a entrada da ESP32 aguenta 3,6 V no
máximo. Em 3V3 o problema deixa de existir na origem, sem divisor nenhum, e o
fundo de escala do sinal passa a bater exatamente com o do ADC. O LM393 funciona
a partir de 2 V; o que se perde é um pouco de alcance do LED infravermelho,
irrelevante a 3–5 mm do chão.

O `XSHUT` de cada olho **nunca pode ficar solto** (§1.4 da datasheet: pino solto
gera corrente de fuga). É ele que permite os dois no mesmo barramento: os dois
nascem em 0x29 e o boot acorda um de cada vez para reendereçar o esquerdo.

---

## 4. Como gravar

**Pelo PC (primeira vez):**
1. Arduino IDE → Gerenciador de placas → *esp32 by Espressif* (2.x ou 3.x, ambos compilam).
2. Bibliotecas: **Adafruit GFX Library** e **Adafruit SSD1306**.
3. Placa `ESP32 Dev Module`, abrir `RoboSumo.ino`, gravar.

**Sem PC (a partir daí):** *Sketch → Export Compiled Binary*, pegue o `.bin`,
conecte no Wi-Fi `ROBO_SUMO` com o celular, abra o painel, seção
**Gravar firmware sem PC (OTA)**, envie o arquivo. A ESP reinicia sozinha.

---

## 5. Painel web

- Rede Wi-Fi **`ROBO_SUMO`**, aberta, sem senha.
- Endereço: **http://192.168.4.1** ou **http://robosumo.local**
- Portal cativo: qualquer endereço digitado cai no painel.

O que dá para fazer lá: armar/parar, trocar de modo, ver o radar dos dois olhos
ToF em tempo real, ver PWM e termos do PID no gráfico, calibrar o limiar dos
sensores IR, ajustar todos os parâmetros com os sliders (valem na hora), salvar
na memória, pilotar manualmente e ler o registro de combate.

O primeiro card é o **Bancada**: ele varre os dois barramentos I2C na hora e diz,
componente por componente, o que respondeu de verdade — endereço do OLED, os dois
olhos, os pinos DO e AO de cada IR, o ADC da bateria e o `nFAULT` das pontes. É o
mesmo autoteste do boot, servido em `GET /api/selftest`, e o botão **REVARRER**
repete a varredura sem resetar a placa. Serve para conferir fiação pelo celular,
com o robô já fechado e longe do cabo USB.

> A verdade do card é elétrica, não otimista: verde só quando algo **respondeu**.
> Um IR com o pino solto aparece em vermelho justamente porque GPIO34/35 não têm
> pull-up interno e leem BAIXO sozinhos — o que pareceria "borda detectada".

Todo ajuste feito nos sliders é **base**; o modo escolhido multiplica em cima.

**Trava de segurança do modo manual:** se o painel ficar 700 ms sem mandar comando
(aba fechada, Wi-Fi caiu, celular bloqueou), o robô para sozinho. Por isso os botões
**TESTAR ESQ / TESTAR DIR** dão um pulso curto de ~0,7 s em vez de sair andando.

---

## 6. Modos

| Modo | O que muda |
|------|-----------|
| **NORMAL** | equilíbrio; os valores base valem como estão |
| **LESMA** | velocidades × 0,45, rampa 2,5× mais longa, +2 leituras para confirmar o alvo, varredura mais lenta. Não se afoba, não passa do ponto |
| **CAPIROTO** | investida em 1000/1000, teto liberado, rampa 30 %, Kp × 1,45, confirma com menos leituras, se compromete a 26 cm, recuo de borda mais curto e mais bruto |
| **CAÇADOR** | alcance de detecção × 1,35 e varredura mais rápida — para arena grande |
| **MURALHA** | quase não gira, segura o centro e só investe quando o alvo entra em 40 cm |
| **DANCINHA** | executa só a coreografia |

---

## 7. Lógica de funcionamento

```
IDLE ──ARMAR──► COUNTDOWN(5 s, bipes) ──► BUSCA
                                            │ viu algo
                                            ▼
                                          TRAVANDO ──confirmou N leituras──► ATAQUE
                                            │ perdeu                          │
                                            └──────────────◄──────────────────┤ perdeu / venceu
                                                                              │
   qualquer estado + IR vê a borda ──► BORDA (freio, recuo, giro) ──► BUSCA ◄──┘
```

**Confirmação anti-ruído.** Um eco isolado não dispara a investida. Para o robô
sair para cima, precisa de `confirmHits` leituras válidas **consecutivas** e o
salto entre leituras precisa ser menor que `jumpCm`. Antes disso ele fica em
TRAVANDO, se aproximando devagar. Isso é o que evita correr atrás de fantasma e
sair da arena por causa de um eco espúrio.

**PID.** O erro é `distânciaEsquerda − distânciaDireita` em cm. Positivo = alvo
à direita. A saída é diferencial: `esq = base + saída`, `dir = base − saída`.
Quando só um sensor enxerga, o erro vira um valor forte e fixo para aquele lado.
Quando o alvo entra em `commitCm` (18 cm, ou 26 no CAPIROTO), a correção é cortada
para 30 % e a base vai ao teto: aí é hora de **empurrar reto**, não de corrigir ângulo.

**Salvamento na borda.** Os pinos DO dos IR ficam em interrupção. A ISR libera um
semáforo e uma task de **prioridade 6 no core 1** acorda na hora — o Wi-Fi e o
OLED estão no core 0 e não conseguem atrasar isso. A sequência é: freio elétrico
(18 ms) → recuo reto em potência cheia → giro para dentro da arena. Borda só do
lado esquerdo gira para a direita, só do direito gira para a esquerda, nos dois
faz meia-volta. Em paralelo, o ADC dos pinos AO confirma a leitura a 200 Hz,
então há dois caminhos independentes detectando a borda.

**Destravamento.** Se estiver empurrando há mais de `stuckMs` sem a distância
diminuir e sem estar colado, o robô recua, angula e volta por outra linha.

---

## 8. Calibração dos sensores IR (faça antes de qualquer luta)

O robô calibra sozinho: você só mostra as duas superfícies a ele.

1. Robô no **centro da arena** (zona escura) → aperte **1. CAPTURAR ESCURO** no
   painel, ou a tecla `d` no monitor serial.
2. Robô **em cima da faixa branca** → aperte **2. CAPTURAR CLARO**, ou a tecla `c`.
3. Pronto. Ele calcula o limiar no meio do caminho e **descobre o sentido do sinal
   pela própria medida** — se o seu módulo indicar borda com valor alto em vez de
   baixo, ele inverte sozinho. Confira e **salve**.

Cada captura é a média de 48 amostras, porque tanto o LM393 do módulo quanto o
ADC do ESP32 tremem algumas contagens.

O firmware reclama sozinho de duas coisas, e vale escutar:

- **`SEPARACAO PEQUENA`** (menos de ~250 contagens entre escuro e claro): o sensor
  está longe demais do chão ou a faixa não é clara o bastante. Com pouca separação,
  qualquer variação de luz ambiente atravessa o limiar e o robô ou inventa borda no
  meio da arena, ou não vê a borda de verdade. Aproxime para 3–5 mm.
- **`OS DOIS LADOS NAO CONCORDAM`**: os dois sensores estão em alturas diferentes.
  Como o limiar é um só para os dois, ele vai servir mal para ambos — acerte a
  altura mecânica antes de aceitar a calibração.

Os sensores IR devem ficar **na frente, nos dois cantos, a 3–5 mm do chão** e
apontados para baixo. Quanto mais à frente, mais cedo o robô descobre a borda —
esse é o metro de segurança inteiro do projeto.

> **Sentido do sinal.** O padrão de fábrica assume borda = valor de ADC **baixo**
> (superfície clara reflete mais IR → fototransistor conduz → tensão cai). Mas isso
> depende do módulo, e é por isso que a calibração mede em vez de supor.

---

## 9. Jogo de rodas e mecânica

O que faz diferença de verdade numa arena de sumô:

- **Tração 4×4 com pneus de silicone.** Os N20 vêm com pneu duro; troque por
  silicone ou passe uma camada de cola quente/silicone líquido na banda. Aderência
  ganha de potência.
- **Rodas afastadas ao máximo na largura, próximas no comprimento.** Base larga
  = difícil de virar; entre-eixos curto = giro sobre o próprio eixo mais rápido
  e mais fiel ao que o PID pediu.
- **Rampa/cunha frontal rasa, quase raspando o chão (0,2–0,5 mm).** É a cunha que
  ganha a luta: ela entra por baixo do oponente e tira a tração dele. Chapa de aço
  fina de 0,8 mm, chanfrada.
- **Peso à frente, centro de gravidade baixo.** Bateria e drivers em cima do eixo
  dianteiro; ESP32 e OLED atrás e no alto (são leves).
- **Olhos ToF divergentes ~15° cada.** Se ficarem paralelos, os dois leem a
  mesma coisa e o erro do PID fica sempre perto de zero — o robô não sabe para
  que lado virar. Divergentes, cada um "cobre" um lado e a diferença vira direção.
  O VL53L0X enxerga num cone de ~25°, bem mais estreito que o de um ultrassônico:
  a angulação importa ainda mais, e um olho torto cria um ponto cego real.
- **Ímãs de neodímio** só se o regulamento da sua categoria permitir.

---

## 10. Sons

Tocador RTTTL não bloqueante (o mesmo formato de toque de celular antigo que se
acha pronto na internet). As melodias embutidas são **de domínio público** ou
composições próprias:

| Slot | Melodia |
|------|---------|
| Batalha (padrão) | *Cavalgada das Valquírias* — Wagner |
| Batalha (CAPIROTO) | *Abertura Guilherme Tell* — Rossini |
| Provocação (LESMA) | *Toureador*, de Carmen — Bizet |
| Vitória curta | Toque militar de carga |
| Vitória longa | *Hino à Alegria* — Beethoven |
| Dancinha | composição própria |
| Boot / borda / travou alvo | efeitos próprios |

Para trocar por outra: cole a string RTTTL no slot correspondente em `sound.h`.

---

## 11. Arquivos

| Arquivo | Conteúdo |
|---------|----------|
| `RoboSumo.ino` | globais, criação das tasks e distribuição entre os núcleos |
| `config.h` | pinout, estados, modos, parâmetros e telemetria |
| `motors.h` | DRV8833 em decaimento lento, rampa, freio |
| `sensors.h` | VL53L0X em barramento I2C próprio, IR com ISR + ADC, bateria, autoteste |
| `brain.h` | máquina de estados, PID, guarda de borda, coreografia |
| `face.h` | carinha animada no OLED |
| `sound.h` | tocador RTTTL e biblioteca de melodias |
| `web.h` | AP aberto, portal cativo, API JSON, OTA, NVS |
| `page.h` | painel HTML/CSS/JS embarcado |
| `docs/esquematico.html` | esquemático, pinout e diagramas (abra no navegador) |

---

## 12. Diagnóstico pela porta serial

Ligue o Monitor Serial em **115200**. O firmware fala sozinho, independente do
painel web, e é por aqui que se resolve problema de fiação.

**No boot** roda um autoteste elétrico. Esta é uma captura real de uma placa
com *nenhum* periférico ligado ainda — é exatamente assim que o "vazio" se parece:

```
[tof] XSHUT dos dois ok (barramento vazio em reset)
[tof] olho ESQUERDO NAO respondeu (0xC0 != 0xEE)
[tof] olho DIREITO NAO respondeu (0xC0 != 0xEE)
[tof] NENHUM olho respondeu - confira SDA/SCL/VIN
[boot] OLED: NAO ENCONTRADO (checar SDA=21 SCL=22 e alimentacao)
[teste] --- autoteste ---
[teste] varrendo I2C do OLED (SDA 21 / SCL 22):  NENHUM DISPOSITIVO
[teste] varrendo I2C dos olhos (SDA 16 / SCL 17):  NENHUM DISPOSITIVO
[teste] IR DO esq (GPIO34): BAIXO | IR DO dir (GPIO35): BAIXO
[teste] AO esq (GPIO32)=494  AO dir (GPIO33)=4095  VBAT (GPIO39)=431 de 4095
[teste] --- fim ---
```

Como ler cada linha:

| Leitura | O que significa |
|---|---|
| `0xC0 != 0xEE` | o firmware lê o registrador 0xC0, que vale 0xEE em todo VL53L0X (Tab. 4 da datasheet). É assim que ele separa "sensor de verdade" de "barramento vazio" — sem essa checagem o `init()` da biblioteca consegue "dar certo" falando com o nada |
| varredura I2C **sem nenhum endereço** | SDA/SCL trocados, módulo sem alimentação, ou o barramento errado (o OLED é `Wire` 21/22; os olhos são `Wire1` 16/17 — são dois barramentos separados de propósito) |
| `0x?? responde com os dois XSHUT em reset` | algum XSHUT não está sendo controlado. Com os dois em reset o barramento tem que estar **vazio** |
| `IR DO ... BAIXO` sem o módulo ligado | GPIO34/35 **não têm pull-up interno**: pino solto lê BAIXO e parece borda para sempre. Só o valor do AO desmente |
| AO travado em **4095** ou em **0** | módulo IR sem alimentação ou AO não conectado — o pino está flutuando no fundo de escala |
| VBAT perto de **0** | divisor 100 k/47 k não ligado. Uma LiPo 2S cheia daria ≈ 3100 de 4095 |

**A cada segundo**, o heartbeat de operação:

```
[OLHO] esq:   43cm (medindo, cru=430mm, falhas=0) | dir:   -1cm (sem alvo no alcance, cru=-1mm, falhas=3)
[IR  ] esq: pino=BAIXO AO=1140 -> BORDA | dir: pino=BAIXO AO=4095 -> seguro
[SYS ] estado=IDLE      vbat=7.42V loop=200Hz heap=201412
```

A distinção que mais importa está entre parênteses no `[OLHO]`:

- `AUSENTE no barramento` — o sensor não subiu no boot. É problema **elétrico**:
  alimentação, SDA/SCL ou XSHUT. Não adianta procurar na lógica.
- `sem alvo no alcance` — o sensor está **vivo e medindo**, só não há nada dentro
  do alcance útil. Normal na bancada.
- `medindo` — tem alvo e a distância vale.

Não precisa resetar a placa a cada fio mexido: a task dos olhos fica sondando o
barramento e reinicializa sozinha quando um sensor aparece, e o OLED sobe na
próxima varredura do painel.

**Atalhos de bancada.** Com o monitor serial aberto, uma tecla dispara teste sem
precisar do painel web (útil justamente enquanto se monta, quando entrar no AP
pelo celular custa mais que apertar uma tecla):

| Tecla | O que faz |
|---|---|
| `t` | padrão de teste no display, 4 etapas de 1,6 s |
| `s` | roda o autoteste de novo — varre os dois barramentos e sobe o OLED se ele acabou de ser plugado |
| `b` | toca o bipe de boot |
| `d` | captura a zona **escura** para a calibração do IR |
| `c` | captura a faixa **clara** |
| `x` | zera a calibração capturada |
| `m` | monitor de IR a **20 Hz por 20 s** — imprime linhas `#IR ms esqAO dirAO esqDO dirDO` |
| `o` | monitor dos **olhos** a 20 Hz por 20 s — linhas `#TOF ms mmL mmR cmL cmR okL okR falhaL falhaR` |
| `e` | **exame dos olhos**: varre o barramento com XSHUT baixo/alto/solto, a 400 e 100 kHz, com SDA/SCL trocados, e mede curto entre os quatro pinos de I2C |
| `w` | põe o **`Wire1` para falar nos pinos do OLED** (21/22). Se achar o dispositivo lá, o periférico está bom e a culpa é da fiação de 16/17. Exige o módulo plugado em 21/22 |
| `a` | arma / desarma |
| `?` | lista os atalhos |

O **monitor de IR (`m`)** existe porque o heartbeat de 1 Hz não distingue as duas
coisas que mais confundem na bancada: módulo reagindo de verdade e pino solto
derivando. A 20 Hz a *forma* do sinal aparece, e ela é decisiva — módulo ligado
responde ao papel com **degraus retangulares repetíveis**; pino flutuando desenha
uma **rampa lenta que não se repete**, porque é o capacitor da entrada do ADC
carregando. Um pino solto também reage à mão por acoplamento capacitivo e parece
sensor vivo: por isso o teste se faz com papel branco, não com a mão.

O autoteste também imprime a **identidade do chip** (`chip=… psram=…`) e confere se
os pinos dos olhos obedecem. Isso não é curiosidade: em placas ESP32-**WROVER** os
GPIO 16 e 17 pertencem à PSRAM e não servem de I2C — e o sintoma seria idêntico ao
de fio solto. Com `psram=0KB` essa hipótese cai na hora, sem trocar um fio.

A **sonda elétrica das linhas** responde o que a varredura I2C não responde. Ela
descarrega a linha e vê quem a levanta: pull-up externo levanta em microssegundos,
linha aberta fica em baixo. Roda sempre com dois controles — o barramento do OLED
(onde sabemos que há módulo) e o GPIO 5, que o projeto não usa. Se o controle
negativo acusar pull-up, a sonda está mentindo e nenhuma conclusão dela vale.

> **A sonda de linha não separa um pull-up de 10 k de um fio encostado no 3V3.**
> Nos dois casos alguém segura a linha em alto. Quem desempata é a varredura I2C —
> e essa distinção já custou uma investigação inteira neste projeto.

> **Pull-up presente não prova sensor acordado.** Os resistores do módulo ficam na
> alimentação dele e existem mesmo com o sensor em reset por XSHUT — e existem até
> quando o módulo se alimenta parasitando as próprias linhas de sinal, que é o que
> acontece quando falta o GND comum.

O **monitor dos olhos (`o`)** serve para julgar o que mais estraga uma investida:
não é distância errada, é distância **instável**. Uma leitura que pula sozinha faz
o PID corrigir para um alvo que não existe. A 20 Hz o tremor aparece; a 1 Hz, não.

O **padrão de teste do display** não é enfeite: a moldura com as diagonais revela
módulo **SH1106** vendido como SSD1306 (ele tem 132 colunas e mostra 128, então a
imagem nasce deslocada 2 px e a moldura aparece cortada); o xadrez de 1 px revela
linha ou coluna morta; a tela toda acesa revela pixel queimado e alimentação fraca;
a última etapa confirma o endereço em que ele respondeu.

Além do texto legível, sai uma linha `#D chave=valor` por segundo para consumo
por máquina — é ela que alimenta o painel de bancada do PC:

```powershell
.\docs\painel.ps1 -Porta COM5
```

---

## 13. Compilar e gravar pela linha de comando

O projeto tem `platformio.ini` e compila tanto pela Arduino IDE quanto pelo
PlatformIO, sem mexer no layout dos arquivos. A porta COM **muda** quando a placa
é reconectada, por isso o `platformio.ini` não fixa nenhuma: se o upload falhar
com *could not open*, confira antes se a placa ainda está enumerada, com
`[System.IO.Ports.SerialPort]::GetPortNames()`.

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run                    # compila
& $pio run --target upload    # grava (porta autodetectada)
& $pio device monitor         # monitor serial
```

---

## 14. Checklist de bancada (antes de encostar na arena)

1. Rodas no ar. Painel → **TESTAR ESQ** / **TESTAR DIR**: cada lado deve girar
   para frente. Se um estiver invertido, troque os dois fios daquele motor no borne.
2. Painel → card **Bancada**: tudo que está montado precisa estar verde antes de
   qualquer outra coisa. Depois aproxime a mão de cada olho e veja o radar responder
   do lado certo.
3. Passe um papel branco sob cada IR e veja o indicador acender.
4. Só então arme.
