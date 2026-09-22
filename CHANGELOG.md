# Changelog

Formato: [Conventional Commits](https://www.conventionalcommits.org/pt-br/) ·
Versionamento: [SemVer](https://semver.org/lang/pt-BR/).

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
