# Changelog

Formato: [Conventional Commits](https://www.conventionalcommits.org/pt-br/) ·
Versionamento: [SemVer](https://semver.org/lang/pt-BR/).

## 4.0.0 — ponte L298N e pinagem de encaixe

### Alterado

- **Ponte: TB6612FNG → L298N (módulo micro).** Mesma topologia de 2 rodas; o
  antigo `STBY` virou `ENA`+`ENB` amarrados num GPIO só.
- **Pinagem rearranjada para a ponte encaixar de uma vez.** `IN1`…`IN4` ficam na
  fileira `5 · 6 · 7 · 10` — uma barra de 6 vias cobrindo 5 → 10, com as posições
  do 8 e do 9 **sem contato**, porque são pinos de strapping. Não existe corrida
  de quatro pinos úteis seguidos nesta placa: o 2 quebra uma fileira, o 8 e o 9
  quebram a outra.
- O `EN` foi para o GPIO 20 e o HC-SR04 ganhou o par adjacente `3`/`4`, então ele
  também entra num conector só.
- `vMax` subiu de 820 para 900, para compensar parte da queda do L298N.

### Consequências da tabela-verdade do L298N

- O PWM continua nos pinos `IN`, não no `EN`: com `EN` em ALTO, chavear `IN2`
  enquanto `IN1` fica em ALTO alterna girar ↔ freio, que é decaimento lento. PWM
  no `EN` alternaria entre girar e roda livre, que dá menos torque parado.
- **Com `EN` em ALTO o L298N não tem roda livre**: `IN1`=`IN2` freia, em ALTO ou
  em BAIXO. Por isso `Mot::coast()` passou a baixar o `EN` — antes ele deixaria a
  ponte freando.

### Montagem

- **Pull-down de 10 kΩ do `EN` para o GND.** Entre ligar e o `setup()` rodar o
  GPIO 20 ainda não é saída e flutua; `EN` flutuando pode habilitar a ponte com
  os `IN` indefinidos, e o sintoma é um tranco nas rodas ao ligar. Agrava que o
  GPIO 20 é o `U0RXD`, mexido pelo bootloader da ROM.
- A lógica do L298N (`Vss`) quer **5 V**, não 3,3 V. Já as entradas pedem
  `Vih ≥ 2,3 V`, então os 3,3 V da C3 comandam a ponte direto — some o aperto de
  margem que a TB6612 tinha quando alimentada em 5 V.
- O L298N derruba cerca de **2 V** na própria ponte: dos 7,8 V o motor vê perto
  de 5,8 V, e o resto vira calor.

## 3.0.0 — eletrônica enxuta

Duas rodas, uma ponte, e a borda lida por tensão em vez de por nível lógico.

### Alterado

- **Tração: 4 motores e 2× DRV8833 → 2 motores e 1× TB6612FNG.** Os 7 pinos da
  ponte não cabiam nos 10 GPIOs úteis do C3 (a conta pedia 11), então `PWMA` e
  `PWMB` vão jumpeados em 3V3 e o PWM é aplicado nos próprios `AIN`/`BIN` — pela
  tabela-verdade da datasheet isso alterna entre CW e *short brake*, que é
  decaimento lento e dá mais torque parado.
- **Borda: saída digital → saída analógica do IR.** O `DO` já vem comparado
  contra o trimpot do módulo, e um limiar que mora num parafuso não dá para
  versionar nem ajustar por conversa. Agora é `IR_LIMIAR_MV`, no topo do
  `config.h` junto do alcance do ultrassom.
- **A guarda de borda deixou de usar interrupção** — não se interrompe em limiar
  analógico. Virou varredura a 1 kHz na task de prioridade 6, que preempta o
  ataque no meio: 1 ms de latência no pior caso.
- **Painel web recuperou o acionamento dos motores e o seletor de modos**,
  adaptados para duas rodas. A pilotagem reenvia a direção a cada 300 ms, o que
  mantém vivo o corte por silêncio do `ST_MANUAL`.
- Bateria passou a 7,8 V, direto no `VM` da ponte. O `VCC` da ponte vai em
  **3,3 V**, não em 5 V: a datasheet pede `VIH = VCC × 0,7`, e com 5 V isso
  exigiria 3,5 V — acima do que a C3 entrega.

### Removido

- `juiz.h` e a biblioteca `IRremoteESP8266`. O GPIO 21 fica mapeado e reservado;
  a decodificação do protocolo entra depois. O firmware caiu de 75% para 63% da
  flash e não tem mais nenhuma dependência externa.
- Detecção de falha da ponte: a TB6612FNG não tem saída de *fault*.

### Montagem

- Dois divisores de tensão novos: **1 k / 2 k** no `ECHO` (5 V → 3,33 V) e
  **10 k / 10 k** no `AO` do IR (5 V → 2,50 V, dentro da faixa linear do ADC).
- **Step-down de 5 V obrigatório** entre a bateria e a ESP32 — o regulador do C3
  SuperMini aceita cerca de 6,5 V e 7,8 V o queima.

## 2.0.0 — eletrônica simplificada

Alguns sensores não se comportaram na bancada, então a eletrônica encolheu.
A lógica de combate é a mesma; o que mudou foi o hardware embaixo dela.

### Alterado

- **Controlador: ESP32 DevKit V1 → ESP32-C3 Mini.** O C3 tem um núcleo só, então
  a separação de tarefas entre núcleos deu lugar a uma hierarquia de
  prioridades. O `xTaskCreatePinnedToCore` do desenho antigo dispararia um
  assert do FreeRTOS no boot.
- **Olhos: 2× VL53L0X → 1× HC-SR04.** O `ECHO` é medido por interrupção, e não
  com `pulseIn()` — numa placa de núcleo único aquela espera ocupada seguraria
  a CPU por até 38 ms por leitura, tempo de sobra para o robô passar da borda.
- **Borda: 2 sensores IR → 1**, montado o mais à frente possível. A fuga passou
  a ser sempre "recua e gira", com o sentido do giro alternando a cada
  salvamento: insistir no mesmo lado faz o robô colher a mesma borda quando está
  preso entre duas.
- **Painel web reduzido a leitura + start/stop.** O ajuste de parâmetros saiu da
  tela: slider de PID no celular parece controle e não é.
- Serial agora por USB CDC nativo, o que libera a UART0 (GPIO 20/21) para o
  receptor do juiz e o `nFAULT`.

### Removido

- PID de direção. Com um olho só não existe diferença esquerda/direita para
  alimentar o erro — ele seria sempre zero.
- OLED SSD1306 e a face (`face.h`), buzzer e as melodias (`sound.h`).
- Porte para Arduino Nano (`nano/`), gravação OTA, calibração de IR por ADC e o
  painel de bancada em PowerShell.

### Mantido

- Máquina de estados: `IDLE → COUNTDOWN → SEARCH → LOCK → ATTACK`, mais `EDGE`,
  `UNSTUCK` e a dancinha da vitória.
- Confirmação anti-ruído do alvo e os perfis de modo.
- Receptor IR do juiz e os três estados do Artigo 33, com o *Stop* definitivo.
- Duas pontes DRV8833 com os canais em paralelo, o que faz duas pontes caberem
  em quatro pinos.

## 1.0.0

Primeira versão fechada: ESP32 DevKit V1, 2× VL53L0X, 2 sensores IR de borda,
OLED, buzzer e painel web com afinação completa.
