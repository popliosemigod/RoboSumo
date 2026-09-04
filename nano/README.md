# Porte bruto — Arduino Nano + NANO Pro Shield

Regressão deliberada de controlador. O objetivo não é ter um robô melhor: é
**separar o que era problema de fiação do que era problema da placa**, trocando
só o cérebro e mantendo todos os periféricos.

O firmware do ESP32 continua intacto na `main` — esta pasta é compilada por um
ambiente próprio e os dois não se misturam.

---

## 1. Pinout

### Digitais

| Pino | Função | Ligar em | Por quê aqui |
|------|--------|----------|--------------|
| **D2** | IR esquerdo · `DO` | módulo IR esq, saída digital | única interrupção externa livre (INT0) |
| **D3** | Buzzer | base do transistor / piezo | `tone()` sequestra o Timer2; fica sozinho nele |
| **D4** | IR direito · `DO` | módulo IR dir, saída digital | interrupção por mudança de pino (PCINT20) |
| **D5** | **IN1** — lado esquerdo | DRV8833 #1 · AIN1 **e** BIN1 | PWM (Timer0) |
| **D6** | **IN2** — lado esquerdo | DRV8833 #1 · AIN2 **e** BIN2 | PWM (Timer0) |
| **D7** | `nSLEEP` das duas pontes | SLP dos dois módulos + pull-down 10 k | |
| **D8** | `nFAULT` das duas pontes | dreno aberto + pull-up 10 k para 5 V | |
| **D9** | **IN3** — lado direito | DRV8833 #2 · AIN1 **e** BIN1 | PWM (Timer1) |
| **D10** | **IN4** — lado direito | DRV8833 #2 · AIN2 **e** BIN2 | PWM (Timer1) |
| **D11** | `XSHUT` do olho **esquerdo** | VL53L0X esq · XSHUT | |
| **D12** | `XSHUT` do olho **direito** | VL53L0X dir · XSHUT | |
| **D13** | LED de bordo | já existe na placa | pisca quando armado |

### Analógicos

| Pino | Função | Ligar em |
|------|--------|----------|
| **A0** | IR esquerdo · `AO` | saída analógica do módulo IR esq |
| **A1** | IR direito · `AO` | saída analógica do módulo IR dir |
| **A2** | Leitura de bateria | nó do divisor 100 k / 47 k |
| **A3** | Botão ARMAR | botão para GND (pull-up interno) |
| **A4** | **SDA** | OLED **e** os dois VL53L0X |
| **A5** | **SCL** | OLED **e** os dois VL53L0X |
| A6, A7 | livres | só entrada analógica, sem função digital |

> **Armadilha do shield:** no NANO Pro Shield o conector rotulado `SDA/SCL` **não
> funciona** — o próprio esquemático dele diz "此版本 SDA SCL 无效" (nesta versão,
> SDA/SCL inválidos). Use os pinos **A4 e A5** nos blocos G/V/S de analógico.

---

## 2. Alimentação

O shield tem regulador **AMS1117-3.3** próprio (`1117-33` no esquemático dele),
alimentado pelo VCC de 5 V. Isso dá um 3V3 com corrente de verdade, coisa que o
3V3 do Nano cru não tem.

```
LiPo 2S 7,4 V ──┬── chave ──┬── VM dos dois DRV8833  (+ 470 µF + 100 nF)
                │           └── divisor 100k/47k ── A2
                └── buck 5 V ──┬── VIN do Nano
                               ├── VCC dos 2 módulos IR
                               ├── VCC do OLED
                               ├── VIN dos 2 VL53L0X   ← ver aviso abaixo
                               └── buzzer
GND: um único ponto comum entre bateria, buck, Nano, drivers e sensores.
```

### ⚠ O ponto que pode queimar sensor

O Nano fala I2C em **5 V**. O VL53L0X aguenta **3,6 V** no máximo em `SDA`, `SCL`
e `XSHUT` (Tab. 6 da datasheet). Isso inverte a recomendação que valia no ESP32:

- **Módulo GY-530 / GY-VL53L0XV2** (o comum, com regulador e conversor de nível
  a bordo): alimente **VIN em 5 V**. O conversor passa a referenciar 5 V e o
  sensor fica protegido. Alimentar em 3V3 com I2C de 5 V é que seria errado.
- **Breakout "pelado"**, sem regulador nem conversor: **não ligue direto.**
  Precisa de conversor de nível bidirecional nas duas linhas, ou o sensor morre.

Como saber qual você tem: olhe o módulo. Se houver um CI de 6 pinos perto dos
pinos de alimentação e um segundo pequeno perto de SDA/SCL, é a versão com
regulador + conversor. Na dúvida, meça `VIN` contra o pino de alimentação do
sensor: se der ~2,8 V com VIN em 5 V, há regulador.

O `XSHUT` sai do Nano em 5 V direto para o sensor — nos módulos com conversor
ele também passa pelo shifter; nos pelados, precisa de divisor.

---

## 3. O que se perdeu nesta regressão

Não é opinião, é o que o chip impõe:

| Recurso no ESP32 | No Nano | Motivo |
|---|---|---|
| Painel web, AP, OTA | **não existe** | sem rádio; a serial vira a única interface |
| Dois núcleos, FreeRTOS | loop único | a guarda de borda vira interrupção + teste no topo do loop, e **nada** no loop pode bloquear |
| Dois barramentos I2C | um só (A4/A5) | OLED divide a linha com os olhos; o desenho é suspenso durante ataque e borda |
| PWM 20 kHz inaudível | 976 Hz (esq) / 490 Hz (dir) | timers do AVR são compartilhados e o Timer0 é o do `millis()` |
| Leitura dos dois olhos junta | alternada, um por ciclo | ler os dois de uma vez custaria ~50 ms de cegueira para a borda |
| Carinha animada | desligada por padrão | ver abaixo |

### O display fica desligado por padrão

Medido, não achismo:

| | Flash | RAM estática |
|---|---|---|
| com OLED | 28 540 / 30 720 (**92,9 %**) | 711 B |
| sem OLED | 17 588 / 30 720 (57,3 %) | 588 B |

O buffer do SSD1306 são **mais 1024 B alocados em tempo de execução**, que não
aparecem nesse número. Com o display ligado sobram ~313 bytes de pilha — e a
pilha carrega chamadas da biblioteca do VL53L0X, `print` de float e quadros de
interrupção. Estourar isso não dá erro: dá **reset aleatório**, exatamente o
sintoma que este porte existe para não ter enquanto diagnostica.

Para ligar assim mesmo: `#define USAR_OLED 1` no topo de `RoboSumoNano.cpp`.

---

## 4. Compilar e gravar

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -e nano                    # compila
& $pio run -e nano --target upload    # grava
& $pio device monitor -e nano         # serial a 115200
```

Se a gravação der timeout, o clone tem o **bootloader antigo**: use `-e nano_old`
(mesmo código, 57600 baud). Não dá para saber pelo olho qual é — é tentativa.

---

## 5. Console serial

A serial a 115200 é a única interface. Uma tecla dispara cada função:

| Tecla | O que faz |
|---|---|
| `a` | arma / desarma |
| `s` | autoteste: varre o I2C e, se não achar ninguém, faz a sonda elétrica de A4/A5 |
| `e` | exame dos olhos: varre com XSHUT em baixo, alto e solto |
| `b` | buzzer |
| `m` | monitor a 20 Hz por 10 s — linhas `#M ms dL dR irLa irRa irLdo irRdo` |
| `1` `2` `3` | modo NORMAL / LESMA / CAPIROTO |
| `?` | ajuda |

A cada segundo sai uma linha `#D chave=valor` com o estado completo.

> **Inversão de sintoma que vai confundir:** aqui D2 e D4 **têm** pull-up interno.
> Pino de IR solto lê **ALTO**, não BAIXO como no ESP32. O sinal de fio solto é o
> oposto do que você aprendeu na outra placa.
