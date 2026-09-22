# Robô Sumô — ESP32-C3 SuperMini

Robô de sumô autônomo para a categoria Mini Sumô do *Sumô Tech Fight* (InovaWeek,
Universidade Vila Velha). Procura o oponente com um ultrassônico, avança e empurra;
o sensor IR vigia a borda da arena e traz o robô de volta antes que ele saia.

![Esquemático](docs/schematic.svg)

## Material

| Qtd | Componente |
|---|---|
| 1 | ESP32-C3 SuperMini + extension board |
| 1 | Ponte H **L298N** (módulo micro) |
| 2 | Motor DC (2 rodas) |
| 1 | HC-SR04 — ultrassônico, acha o oponente |
| 1 | Módulo IR — **saída analógica (AO)**, vigia a borda |
| 1 | Bateria 7,8 V |
| 1 | Step-down 5 V (ver *Alimentação*) |
| — | Resistores: 1 kΩ, 2 kΩ, 2× 10 kΩ (divisores) e 10 kΩ (pull-down do EN) |
| — | Receptor do controle do edital — pino reservado, ainda não montado |

## Pinagem

A pinagem foi arrumada para a ponte H **encaixar de uma vez**, num conector só.

### Fileira A — `5 · 6 · 7 · 8 · 9 · 10 · 20 · 21`

| GPIO | Sinal | Vai para |
|---|---|---|
| **5** | `IN1` | L298N — canal A (roda esquerda) |
| **6** | `IN2` | L298N — canal A (roda esquerda) |
| **7** | `IN3` | L298N — canal B (roda direita) |
| ~~8~~ | — | **strapping — sem contato no conector** |
| ~~9~~ | — | **strapping (BOOT) — sem contato no conector** |
| **10** | `IN4` | L298N — canal B (roda direita) |
| **20** | `ENA` + `ENB` | L298N — ALTO habilita as duas pontes |
| **21** | reservado | receptor do controle do edital |

`IN1` a `IN4` ocupam uma barra de **6 vias** cobrindo 5 → 10, com as posições do
8 e do 9 **sem pino no conector**.

Por que essas duas ficam vazias: são pinos de strapping, e o nível deles no
instante do reset decide de onde a placa dá boot. O 9 é o `BOOT` — se algo o
puxar para baixo na hora de ligar, a placa entra em modo de gravação e o firmware
nunca roda. **Não existe corrida de quatro pinos úteis seguidos nesta placa**: o 2
quebra a fileira B, o 8 e o 9 quebram a A. Três seguidos é o máximo, e é por isso
que o `IN4` pula para o 10 em vez de continuar a sequência.

### Fileira B — `0 · 1 · 2 · 3 · 4`

| GPIO | ADC | Sinal | Vai para |
|---|---|---|---|
| **0** | **ADC1_CH0** | `AO` | IR de borda — **via divisor 10 k / 10 k** |
| **1** | ADC1_CH1 | — | livre |
| ~~2~~ | — | — | strapping |
| **3** | — | `TRIG` | HC-SR04 — 3,3 V direto, sem divisor |
| **4** | — | `ECHO` | HC-SR04 — **via divisor 1 k / 2 k** |

O HC-SR04 ficou com o par adjacente 3/4, então ele também entra de uma vez, num
conector de 2 vias.

### L298N — pinagem do módulo

| Pino do módulo | Liga em |
|---|---|
| `VMS` / `+12V` / `Vs` | **bateria 7,8 V** |
| `+5V` (`Vss`, lógica) | **trilho de 5 V** — ver o aviso abaixo |
| `GND` | GND comum |
| `IN1` | GPIO 5 |
| `IN2` | GPIO 6 |
| `IN3` | GPIO 7 |
| `IN4` | GPIO 10 |
| `ENA` | GPIO 20 |
| `ENB` | GPIO 20 — **mesmo fio do ENA** |
| `OUT1` / `OUT2` | motor esquerdo |
| `OUT3` / `OUT4` | motor direito |

`ENA` e `ENB` vão juntos porque nunca foi preciso desligar um lado sozinho — e
amarrar os dois economiza um GPIO.

> **A lógica do L298N quer 5 V, não 3,3 V.** O `Vss` do CI é especificado para
> 4,5 a 7 V. Isso é a *alimentação*; as *entradas* são outra coisa, e nelas o
> L298N pede `Vih ≥ 2,3 V` — os 3,3 V da C3 passam com folga, sem level shifter.

> **Ponha um pull-down de 10 kΩ do `EN` para o GND.** Entre aplicar energia e o
> `setup()` rodar existe quase um segundo em que o GPIO 20 ainda não é saída: ele
> flutua, e `EN` flutuando pode habilitar a ponte com os `IN` em estado
> indefinido. O sintoma é um tranco nas rodas toda vez que liga. O resistor
> amarra o `EN` em BAIXO nesse intervalo. Vale ainda mais aqui porque o GPIO 20 é
> o `U0RXD`, e o bootloader da ROM mexe nele antes do nosso código existir.

## Divisores de tensão

A ESP32-C3 é 3,3 V e **não tolera 5 V nos GPIO**. Os dois sensores operam em 5 V,
então cada um tem o seu divisor. `R1` fica em série com o sinal, `R2` vai do nó
para o GND, e o GPIO se conecta ao nó.

```
 Vout = Vin × R2 / (R1 + R2)
```

### 1. `ECHO` do HC-SR04 → GPIO 4

```
   ECHO (5 V) ──[ R1 = 1 kΩ ]──┬── GPIO 4
                               │
                          [ R2 = 2 kΩ ]
                               │
                              GND
```

`Vout = 5 V × 2k / (1k + 2k) = 3,33 V` — abaixo dos 3,6 V de máximo absoluto do GPIO.

### 2. `AO` do IR → GPIO 0 (ADC1)

```
   AO (5 V) ──[ R1 = 10 kΩ ]──┬── GPIO 0  (ADC1_CH0)
                              │
                         [ R2 = 10 kΩ ]
                              │
                             GND
```

`Vout = 5 V × 10k / (10k + 10k) = 2,50 V`

**Por que não 1 k / 2 k aqui também:** o `ECHO` é digital e só precisa caber
abaixo de 3,6 V. Este é analógico, e o ADC da C3 satura perto de 3,1 V — parar em
2,5 V mantém a escala inteira dentro da faixa linear.

**O divisor divide também o limiar.** `IR_LIMIAR_MV` é a tensão medida **no
GPIO**, já dividida. Com razão de 0,5, um limiar de 1600 mV no código corresponde
a 3,2 V na saída do módulo.

## Alimentação

| Trilho | Origem | Alimenta |
|---|---|---|
| **7,8 V** | bateria, direto | `Vs` do L298N (motores) |
| **5 V** | step-down a partir da bateria | `Vss` do L298N, pino `5V` da ESP32, `VCC` do HC-SR04, `VCC` do IR |
| **GND** | — | um único ponto comum: bateria, ponte, sensores e ESP |

> **O step-down não é opcional.** A bateria **não pode** ir direto na ESP32: o
> regulador do C3 SuperMini (ME6211) aceita cerca de 6,5 V, e 7,8 V queima o
> componente. Na bancada dá para pular isso alimentando a ESP32 pelo cabo USB,
> com a bateria ligada só no `Vs` da ponte.

> Se o seu módulo L298N tiver o regulador de 5 V embarcado com jumper (muitos
> têm, e ele funciona com `Vs` até 12 V), esse 5 V pode substituir o step-down.
> Confira no módulo antes de contar com isso.

> **O GND comum não é detalhe de alimentação** — é a referência contra a qual
> "alto" e "baixo" existem. Um módulo com o terra fora do comum já custou uma
> sessão inteira de diagnóstico neste projeto.

**O que o L298N custa:** ele é Darlington bipolar e derruba cerca de **2 V** na
própria ponte, mais sob carga. Dos 7,8 V da bateria o motor vê perto de 5,8 V, e
a diferença vira calor no dissipador. Uma ponte MOSFET (TB6612, DRV8833) derruba
~0,5 V no mesmo lugar.

## Ajuste do comportamento

Os três números que mais mudam ficam no **topo do [`config.h`](config.h)**:

| Constante | Padrão | O que faz |
|---|---|---|
| `IR_LIMIAR_MV` | `1600` | mV no GPIO que separam o piso preto da faixa branca |
| `IR_BORDA_ABAIXO` | `1` | 1 = a faixa branca é a leitura **mais baixa** |
| `US_ALCANCE_CM` | `60` | até onde um eco conta como oponente |

**Como calibrar o IR:** com o monitor serial aberto, `p` mede o piso preto e `b`
mede a faixa branca (600 ms cada, e avisa se a leitura estiver instável). Ponha
`IR_LIMIAR_MV` no meio das duas medidas e regrave.

## Lógica

A ordem de prioridade é o desenho do firmware, não uma sequência de `if`:

1. **Borda** — o IR é a trava de segurança. Uma task de prioridade 6 varre o ADC
   a 1 kHz e, ao ver a faixa clara, preempta o ataque no meio: freio, recuo e
   giro para dentro. O sentido do giro alterna a cada salvamento, para o robô não
   colher a mesma borda quando está preso entre duas.
2. **Alvo** — enquanto o HC-SR04 vê alguém à frente e o IR não vê borda, avança
   em cima e empurra. Abaixo de 18 cm vai com tudo.
3. **Busca** — sem alvo e sem borda, gira varrendo.

**Como as direções saem de dois motores:** tração diferencial. Frente e ré são as
duas rodas iguais; girar é uma roda contra a outra. O PWM vai nos pinos `IN` e o
`EN` fica fixo em ALTO — pela tabela-verdade do L298N isso alterna entre girar e
frear (decaimento lento), que dá mais torque parado do que PWM no `EN`, que
alternaria entre girar e roda livre. Num sumô o que decide a partida é o empurrão
parado.

Modos de combate: `NORMAL`, `LESMA`, `CAPIROTO`, `CACADOR`, `MURALHA`, `DANCINHA`
— multiplicam a afinação base e trocam pelo painel ou pela tecla `m`.

## Compilar e gravar

Toolchain: **PlatformIO**, board `esp32-c3-devkitm-1`. Nenhuma biblioteca externa.

```bash
pio run                  # compila
pio run --target upload  # grava (porta autodetectada)
pio device monitor       # 115200 baud
```

O C3 SuperMini não tem chip USB-serial: o USB é nativo do MCU. É por isso que o
`platformio.ini` liga `ARDUINO_USB_CDC_ON_BOOT` — sem essa flag o Serial não
aparece, e esse silêncio costuma ser confundido com firmware travado no boot.

## Painel web

Ao ligar, o robô sobe um ponto de acesso aberto:

- Rede **`ROBO_SUMO`** (sem senha)
- `http://192.168.4.1` ou `http://robosumo.local`

Mostra distância, eco cru, leitura do IR em mV contra o limiar, PWM das rodas e o
estado da máquina de estados. Aciona: start/stop, os seis modos e a pilotagem
manual (frente, ré, girar para cada lado, parar). Não há ajuste de parâmetro na
tela — a afinação é feita no código.

A pilotagem tem corte por silêncio: se a aba fechar ou o Wi-Fi cair com o robô
andando, ele para sozinho em 700 ms.

## Serial

115200 baud, uma linha por segundo com o olho, o IR e o sistema.

| Tecla | Ação |
|---|---|
| `s` | autoteste |
| `a` | armar / parar |
| `m` | próximo modo |
| `p` | medir o piso preto |
| `b` | medir a faixa branca |
| `?` | ajuda |

Leia o autoteste antes de suspeitar da lógica — a maior parte dos problemas deste
projeto foi fiação, não software.

## Pendente

O receptor do controle do edital tem o **GPIO 21 reservado e nada mais**: a
decodificação do protocolo ainda não existe, então hoje quem arma o robô é o
painel web ou a tecla `a`. Enquanto isso não entrar, o robô não atende ao artigo
do regulamento que exige o receptor do juiz.
