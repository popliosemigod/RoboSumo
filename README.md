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
| 2 | HC-SR04 | ultrassônicos frontais, divergentes ~30° |
| 2 | Sensor IR reflexivo (TCRT5000 / módulo de linha) | com saída **DO** e **AO** |
| 1 | OLED SSD1306 128×64 I2C | endereço 0x3C |
| 1 | Buzzer passivo 5 V | passivo — o firmware gera a frequência |
| 1 | Transistor 2N2222 + resistor 1 k | driver do buzzer (opcional se piezo pequeno) |
| 1 | Regulador buck 5 V 2 A (MP1584 / LM2596) | alimenta ESP32 e sensores |
| 1 | Bateria LiPo 2S 7,4 V ≥ 900 mAh 25C | alimenta os DRV8833 direto |
| 1 | Chave gangorra + botão momentâneo | liga/desliga e ARMAR |
| — | 2× 1 kΩ, 2× 2 kΩ, 2× 10 kΩ, 100 kΩ, 47 kΩ | divisores e pull-up/down |
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
                               ├── VCC dos 2 HC-SR04
                               ├── VCC dos 2 módulos IR
                               └── VCC do buzzer
3V3 da ESP32 ──┬── VCC do OLED
               └── pull-up de 10 k do nFAULT
GND: um único ponto comum entre bateria, buck, ESP32, drivers e sensores.
```

Os motores são de 6 V e a bateria dá 7,4 V. Quem protege é o parâmetro
**TETO DE VELOCIDADE (`vMax`)** — em 820/1000 o motor vê ≈ 6,1 V médios.
No modo CAPIROTO o teto sobe para 1000; use só em lutas curtas.

Os `ECHO` dos HC-SR04 saem em 5 V e **precisam** do divisor 1 k / 2 k
(ECHO → 1 k → GPIO → 2 k → GND). Sem isso você queima a entrada da ESP32.

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

O que dá para fazer lá: armar/parar, trocar de modo, ver o radar dos dois
ultrassônicos em tempo real, ver PWM e termos do PID no gráfico, calibrar o
limiar dos sensores IR, ajustar todos os parâmetros com os sliders (valem na
hora), salvar na memória, pilotar manualmente e ler o registro de combate.

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

1. Abra o painel, olhe o card **Sensores de borda**.
2. Robô no centro (zona escura) → anote os dois valores.
3. Robô em cima da faixa branca → anote de novo.
4. Coloque o slider **LIMIAR IR** na média entre os dois. Salve.
5. Se os indicadores acenderem invertidos, desmarque **"Sensor indica borda com nível BAIXO"**.

Os sensores IR devem ficar **na frente, nos dois cantos, a 3–5 mm do chão** e
apontados para baixo. Quanto mais à frente, mais cedo o robô descobre a borda —
esse é o metro de segurança inteiro do projeto.

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
- **Ultrassônicos divergentes ~15° cada.** Se ficarem paralelos, os dois leem a
  mesma coisa e o erro do PID fica sempre perto de zero — o robô não sabe para
  que lado virar. Divergentes, cada um "cobre" um lado e a diferença vira direção.
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
| `sensors.h` | HC-SR04 por interrupção, IR com ISR + ADC, bateria |
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

**No boot** roda um autoteste elétrico dos pinos:

```
[teste] ECHO esquerdo (GPIO18) com pull-up: ALTO   -> pino livre: divisor ausente ou fio solto
[teste] ECHO direito  (GPIO16) com pull-up: BAIXO  -> puxado para GND: divisor montado e sensor SEM 5 V
[teste] IR DO esq (GPIO34): BAIXO | IR DO dir (GPIO35): BAIXO
[teste] AO esq (GPIO32)=0  AO dir (GPIO33)=0  VBAT (GPIO39)=4067 de 4095
```

Como ler cada linha:

| Leitura | O que significa |
|---|---|
| ECHO com pull-up dá **ALTO** | nada segura o pino: o resistor de 2 k do divisor não está no circuito, ou o fio não chega na ESP32 |
| ECHO com pull-up dá **BAIXO** | há caminho de baixa impedância para o GND — normal quando o divisor está montado e o sensor está **sem 5 V** |
| VBAT perto de **4095** | o divisor 100 k/47 k não está entregando tensão; uma LiPo 2S daria ≈ 3100 |
| AO = **0** nos dois | módulos IR sem alimentação ou AO não conectado |

**A cada segundo**, o heartbeat de operação:

```
[US ] esq:  -1cm eco=PINO MUDO (fiacao?)   falhas=89  | dir: ...
[IR ] esq: pino=BAIXO AO=   0 -> BORDA     | dir: ...
[SYS] estado=IDLE      vbat=10.27V loop=200Hz heap=203180
```

A distinção mais importante está no campo `eco`:

- `PINO MUDO (fiacao?)` — o pino ECHO **nunca subiu**. Um HC-SR04 alimentado
  sempre emite pulso, mesmo sem alvo. Nunca subir é problema elétrico.
- `sem alvo no alcance` — o pino subiu e não desceu a tempo: o sensor está
  **vivo**, só não há nada dentro do alcance. Isso é normal na bancada.

---

## 13. Compilar e gravar pela linha de comando

O projeto tem `platformio.ini` e compila tanto pela Arduino IDE quanto pelo
PlatformIO, sem mexer no layout dos arquivos.

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run                    # compila
& $pio run --target upload    # grava (porta COM3)
& $pio device monitor         # monitor serial
```

---

## 14. Checklist de bancada (antes de encostar na arena)

1. Rodas no ar. Painel → **TESTAR ESQ** / **TESTAR DIR**: cada lado deve girar
   para frente. Se um estiver invertido, troque os dois fios daquele motor no borne.
2. Aproxime a mão de cada ultrassônico e veja o radar responder do lado certo.
3. Passe um papel branco sob cada IR e veja o indicador acender.
4. Só então arme.
