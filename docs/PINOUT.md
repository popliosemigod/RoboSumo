# Pinout oficial — Robô Sumô (ESP32 DevKit V1)

Categoria **Mini Sumô** do *Sumô Tech Fight* / InovaWeek 2026 — 10 × 10 cm,
máx. 500 g, autônomo.

Esta tabela lista **apenas o que está montado**. Não há divisor de bateria,
não há sondas de diagnóstico e não há LED de painel: se não está aqui, não
existe no robô.

---

## 1. Digitais

| GPIO | Sinal | Liga em | Observação |
|---|---|---|---|
| **25** | `IN1` | DRV8833 **esquerdo** — `AIN1` **e** `BIN1` | canais em paralelo |
| **26** | `IN2` | DRV8833 **esquerdo** — `AIN2` **e** `BIN2` | canais em paralelo |
| **27** | `IN3` | DRV8833 **direito** — `AIN1` **e** `BIN1` | canais em paralelo |
| **14** | `IN4` | DRV8833 **direito** — `AIN2` **e** `BIN2` | canais em paralelo |
| **13** | `nSLEEP` | `SLP` das **duas** pontes | + pull-down 10 k → nasce dormindo |
| **15** | `nFAULT` | `FLT` das **duas** pontes | dreno aberto + pull-up 10 k para **3V3** |
| **21** | `SDA` | 2× VL53L0X **e** OLED | barramento único |
| **22** | `SCL` | 2× VL53L0X **e** OLED | barramento único |
| **19** | `XSHUT` | VL53L0X **esquerdo** | reendereça o olho esquerdo em 0x30 |
| **18** | `XSHUT` | VL53L0X **direito** | fica no 0x29 de fábrica |
| **23** | `OUT` | módulo IR de borda **esquerdo** | pull-up interno |
| **16** | `OUT` | módulo IR de borda **direito** | pull-up interno |
| **32** | `OUT` | **receptor IR de 38 kHz do juiz** | Artigo 18 — obrigatório |
| **4** | — | buzzer (via NPN, ou piezo direto) | melodias e contagem |

## 2. Alimentação

| Trilho | Alimenta |
|---|---|
| **LiPo 2S 7,4 V** → chave | `VM` dos dois DRV8833 (+ 470 µF + 100 nF) |
| **3V3** | 2× VL53L0X, OLED, 2× IR de borda, receptor IR do juiz |
| **GND** | um único ponto comum: ESP32, pontes, sensores, bateria |

O `GND` comum não é detalhe de alimentação — é a referência contra a qual
"alto" e "baixo" existem. Um módulo com o terra fora do comum já custou uma
sessão inteira de diagnóstico neste projeto.

## 3. Pinos livres

`2`, `5`, `12`, `17`, `33`, `34`, `35`, `36`, `39`.

`12` é pino de strapping — deixar livre. `34`–`39` são só entrada e **não têm
pull-up interno**: fio solto ali repousa em BAIXO e se disfarça de sinal
válido. É por isso que os dois IR de borda foram para pinos com pull-up.

---

## 4. Regras que o pinout atende

**Artigo 18 — receptor obrigatório.** Receptor de 950 nm / 38 kHz no GPIO 32,
montado no alto do robô com vista livre para o controle do juiz.

**Artigo 33 — três estados.** `A` (0x0C) = *Ready*, `B` (0x18) = *Start*,
`C` (0x5E) = *Stop*. O robô nasce imóvel e só se move no *Start*.

**Artigo 33 — o Stop é definitivo.** Depois do *Stop* o robô não volta para
*Ready* nem aceita *Start*: precisa ser desligado e ligado. No firmware isso é
uma trava sem caminho de volta (`Juiz::travado`).

**Artigo 3 §3 e Artigo 41 — nenhum sinal externo além do juiz.** O ponto de
acesso Wi-Fi sai do ar assim que o juiz manda *Ready*, e só volta com o robô
reiniciado. A infração deixa de ser possível por construção.

**Artigo 5 — o dojô.** 77 cm de diâmetro, preto fosco, com a **Tawara** branca
de 2,5 cm na borda. Os IR de borda procuram exatamente essa faixa clara:
superfície clara leva o `OUT` para BAIXO, e é isso que dispara a fuga.

**Artigo 8 — iluminação.** Haverá período de calibração na arena real. O
limiar mora no trimpot de cada módulo, não no firmware.
