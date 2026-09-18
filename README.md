# Robô Sumô — ESP32-C3

Robô de sumô autônomo para a categoria Mini Sumô do *Sumô Tech Fight* (InovaWeek,
Universidade Vila Velha). Procura o oponente com um ultrassônico, empurra, e foge
da borda quando o sensor IR vê a faixa branca da arena.

![Esquemático](docs/schematic.svg)

## Material

| Qtd | Componente |
|---|---|
| 1 | ESP32-C3 Mini (SuperMini) |
| 1 | HC-SR04 — ultrassônico, detecta o oponente |
| 1 | Módulo IR digital — detecta a borda da arena |
| 1 | Receptor IR 950 nm / 38 kHz — comandos do juiz (Artigo 18) |
| 2 | Ponte H DRV8833 |
| 4 | Motor DC (N20 6 V) |
| 1 | Bateria 7,4 V (2S) + chave geral |

## Pinagem

| GPIO | Sinal | Vai para |
|---|---|---|
| **0** | `IN1` | DRV8833 #1 (esquerda) — `AIN1` + `BIN1` |
| **1** | `IN2` | DRV8833 #1 (esquerda) — `AIN2` + `BIN2` |
| **3** | `IN3` | DRV8833 #2 (direita) — `AIN1` + `BIN1` |
| **4** | `IN4` | DRV8833 #2 (direita) — `AIN2` + `BIN2` |
| **5** | `nSLEEP` | `SLP` das duas pontes (pull-down 10 k → nasce dormindo) |
| **6** | `TRIG` | HC-SR04 |
| **7** | `ECHO` | HC-SR04 |
| **10** | `OUT` | módulo IR de borda (pull-up interno) |
| **20** | `OUT` | receptor IR do juiz |
| **21** | `nFAULT` | `FLT` das duas pontes (dreno aberto + pull-up 10 k) |

**Livres:** 2, 8 e 9 — são pinos de strapping e decidem de onde a placa dá boot.
Carregar um deles faz a placa não subir, e o sintoma não parece elétrico, parece
firmware quebrado. 18 e 19 são o USB nativo.

Duas pontes cabem em quatro pinos porque os dois canais de cada placa entram em
paralelo (`AIN`∥`BIN` e `AOUT`∥`BOUT`): 2 pinos comandam um lado inteiro, e os
dois motores daquele lado recebem literalmente a mesma tensão.

**Alimentação:** bateria 7,4 V → `VM` das duas pontes, e só isso. A ESP32 é
alimentada pelo USB nesta montagem, e o `3V3` dela alimenta HC-SR04, IR de borda
e receptor do juiz. **GND comum** entre bateria, pontes, ESP32 e sensores.

> **Não ligue a bateria no pino `5V` da ESP32.** O regulador do C3 Mini (ME6211)
> aceita no máximo ~6,5 V, e uma 2S entrega 8,4 V carregada. Para o robô andar
> sem cabo USB, use um step-down de 5 V entre a bateria e esse pino.

> O HC-SR04 é especificado para 5 V e em 3,3 V rende menos alcance. Em troca o
> `ECHO` sai em 3,3 V e entra direto no GPIO. Se ele for para 5 V, o `ECHO` passa
> a precisar de divisor de tensão.

## Compilar e gravar

Toolchain: **PlatformIO**, board `esp32-c3-devkitm-1`.

```bash
pio run                  # compila
pio run --target upload  # grava (porta autodetectada)
pio device monitor       # 115200 baud
```

A porta COM muda quando a placa é reconectada — por isso o `platformio.ini` não
fixa `upload_port`.

O C3 Mini não tem chip USB-serial: o USB é nativo do MCU. É por isso que o
`platformio.ini` liga `ARDUINO_USB_CDC_ON_BOOT`; sem essa flag o Serial não
aparece, e esse silêncio costuma ser confundido com firmware travado no boot.

## Painel web

Ao ligar, o robô sobe um ponto de acesso aberto:

- Rede **`ROBO_SUMO`** (sem senha)
- `http://192.168.4.1` ou `http://robosumo.local`

O painel mostra distância do ultrassônico, eco cru, estado do sensor de borda,
PWM dos motores, estado da máquina de estados e o estado do juiz — mais um botão
start/stop. Não há ajuste de parâmetros pela tela: a afinação é feita no código.

O ponto de acesso **sai do ar** assim que o juiz manda *Ready*, e só volta com o
robô reiniciado — durante os rounds o regulamento só admite o sinal do controle
do juiz (Artigo 3 §3).

## Comandos do juiz (Artigo 33)

| Tecla | Estado |
|---|---|
| `A` | *Ready* — pronto e imóvel |
| `B` | *Start* — começa a luta |
| `C` | *Stop* — parada definitiva até desligar e ligar |

## Serial

A 115200 baud, uma vez por segundo: leitura do ultrassônico (com o eco cru), do
sensor de borda e do sistema. Atalhos: `s` = autoteste, `a` = armar/parar,
`?` = ajuda.

Leia o autoteste antes de suspeitar da lógica — a maior parte dos problemas deste
projeto foi fiação, não software.
