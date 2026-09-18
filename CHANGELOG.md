# Changelog

Formato: [Conventional Commits](https://www.conventionalcommits.org/pt-br/) ·
Versionamento: [SemVer](https://semver.org/lang/pt-BR/).

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
