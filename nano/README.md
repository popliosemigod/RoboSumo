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
| **D2** | IR esquerdo · `OUT` | módulo IR esq, saída do comparador | única interrupção externa livre (INT0) |
| **D3** | Buzzer | base do transistor / piezo | `tone()` sequestra o Timer2; fica sozinho nele |
| **D4** | IR direito · `OUT` | módulo IR dir, saída do comparador | interrupção por mudança de pino (PCINT20) |
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
| **A0** | *livre* | nada — o módulo IR é de três fios (ver 1.1) |
| **A1** | *livre* | idem, lado direito |
| **A2** | Leitura de bateria | nó do divisor 100 k / 47 k |
| **A3** | *livre* | o botão de armar saiu do projeto |
| **A4** | **SDA** | OLED **e** os dois VL53L0X |
| **A5** | **SCL** | OLED **e** os dois VL53L0X |
| **A6** | *sonda* — `VCC` do módulo ToF | só diagnóstico, tecla `k` |
| **A7** | *sonda* — `GND` do módulo ToF | só diagnóstico, tecla `k` |

A6 e A7 são as únicas entradas **só analógicas** do Nano: não têm função
digital e não serviriam para mais nada. Por isso viraram voltímetro. São
**sondas de alta impedância** — leem tensão, não alimentam nada. Podem sair
quando o barramento estiver fechado.

### 1.1 Módulo IR de três fios — o caso deste projeto

Os módulos em uso têm **VCC, GND e `OUT`** — três fios, sem saída analógica.
`OUT` é a saída do comparador, o equivalente ao `DO` dos módulos de quatro pinos.

- `OUT` esquerdo → **D2**, `OUT` direito → **D4**
- **A0 e A1 ficam livres.** Não há o que ligar neles.
- O firmware nasce com `irSource = 0` (só o pino digital). Com módulo de três
  fios qualquer outro valor faz o robô decidir borda a partir de pino solto.

**O limiar deixa de ser software.** Quem decide claro/escuro é o **trimpot do
próprio módulo**, não mais um parâmetro no painel. O procedimento:

1. Sensor a 3–5 mm sobre a **zona escura**.
2. Tecla `p` no serial: 60 s imprimindo o estado dos dois `OUT` a 20 Hz.
3. Gire o trimpot até o `OUT` **virar de estado** — é o ponto de disparo.
4. Recue um pouco, para ficar com margem do lado "seguro".
5. Passe sobre a **faixa clara** e confira que ele vira, e volta ao tirar.

Se o indicador acender invertido (borda onde deveria ser zona segura), o módulo
indica com nível oposto: mude `irActiveLow` para 0 em `RoboSumoNano.cpp`.

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
                               ├── VCC dos 2 módulos IR      (OUT sai em 5 V, ok para o Nano)
                               ├── VCC do OLED
                               ├── VIN dos 2 VL53L0X   ← ver aviso abaixo
                               └── buzzer
GND: um único ponto comum entre bateria, buck, Nano, drivers e sensores.
```

### ⚠ O ponto que pode queimar sensor

**Módulo em uso: CJMCU-53L0X V2** (placa roxa), pinagem `VCC · GND · SCL · SDA ·
GPIO1 · XSHUT`. `GPIO1` é a saída de interrupção do sensor e fica sem ligar —
a datasheet diz explicitamente "GPIO1 to be left unconnected if not used".

O Nano fala I2C em **5 V**. O silício do VL53L0X aguenta **3,6 V** no máximo em
`SDA`, `SCL` e `XSHUT` (Tab. 6 da datasheet). Ou seja: a recomendação **inverte**
em relação ao ESP32, e depende de o módulo ter conversor de nível a bordo.

**Meça antes de ligar no Nano. Dois minutos, e é decisivo:**

1. Alimente **só o módulo**: `VCC` em 5 V, `GND` no GND. Nada mais conectado.
2. Meça a tensão contínua entre o pino **`SDA`** e o GND, com o módulo parado.

| Leitura | O que significa | O que fazer |
|---|---|---|
| ≈ **5 V** | há conversor de nível, e os pull-ups referenciam o VCC | ligue direto no Nano com `VCC` em **5 V** |
| ≈ **2,8 V** | os pull-ups estão do lado do sensor, sem conversor | **não ligue direto** — precisa de conversor bidirecional em SDA e SCL, e divisor no XSHUT |

O raciocínio: em repouso o barramento fica em alto puxado pelos pull-ups. A
tensão em que ele repousa denuncia de que lado do conversor esses resistores
estão. É a mesma pergunta que a sonda de linha do firmware responde, só que
aqui feita antes de energizar a lógica.

Alimentar o módulo em 3V3 **não** é o caminho seguro aqui: o que queima o sensor
não é o VCC, é o nível de 5 V que o Nano coloca nas linhas de sinal.

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
| `i` | **identidade dos olhos**: lê 0xC0/0xC1/0xC2 e confirma que é mesmo um VL53L0X |
| `b` | buzzer |
| `m` | monitor a 20 Hz por 10 s — linhas `#M ms dL dR irLa irRa irLdo irRdo` |
| `p` | 60 s de leitura contínua para **ajustar o trimpot** do IR |
| `1` `2` `3` | modo NORMAL / LESMA / CAPIROTO |
| `?` | ajuda |

A cada segundo sai uma linha `#D chave=valor` com o estado completo.

### Por que existe a tecla `i`

A varredura I2C responde "alguém respondeu em 0x29" — não responde "e esse
alguém é um VL53L0X". A Tab. 4 da datasheet dá três registradores de identidade
que existem desde o reset e nunca mudam: `0xC0=0xEE`, `0xC1=0xAA`, `0xC2=0x10`.
Lendo os três, módulo trocado, endereço coincidente de outro periférico e sensor
morto que ainda dá ACK deixam de se parecer com sensor bom.

> **Inversão de sintoma que vai confundir:** aqui D2 e D4 **têm** pull-up interno.
> Pino de IR solto lê **ALTO**, não BAIXO como no ESP32. O sinal de fio solto é o
> oposto do que você aprendeu na outra placa.
