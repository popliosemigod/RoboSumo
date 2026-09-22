# Instruções do projeto — Robô Sumô (ESP32-C3)

## O que é

Firmware de um robô de sumô autônomo da categoria Mini Sumô. **ESP32-C3
SuperMini** com extension board, 2 motores DC, 1 ponte **L298N** (módulo micro),
1 HC-SR04 (o "olho") e 1 sensor IR de borda lido pela **saída analógica**. O
painel de controle é servido pela própria placa, num ponto de acesso Wi-Fi aberto.

A pinagem é feita para encaixar: `IN1`…`IN4` ocupam a fileira `5 · 6 · 7 · 10`
(as posições do 8 e do 9 ficam sem contato no conector) e o HC-SR04 fica no par
adjacente `3`/`4`.

A pinagem oficial, os divisores de tensão e a lista de material estão no
[README.md](README.md); o esquemático em [docs/schematic.svg](docs/schematic.svg).
**Só entra nesses documentos hardware que está fisicamente montado** — sonda de
diagnóstico e circuito planejado não aparecem como se existissem.

## Como compilar e gravar

O toolchain é **PlatformIO**, e a placa fica ligada por USB nesta máquina.
Compilar e gravar é responsabilidade do agente, não do usuário.

Nesta máquina o atalho `pio.exe` não existe — invocar pelo Python do venv:

```powershell
$py = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $py -m platformio run                   # compila
& $py -m platformio run --target upload   # grava (porta autodetectada)
```

A porta COM **muda** quando a placa é reconectada. Por isso `platformio.ini` não
fixa a porta. Se o upload falhar com "could not open", confira se a placa ainda
está enumerada antes de suspeitar do código:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

## Como depurar

A serial a 115200 é a fonte de verdade — funciona mesmo com o Wi-Fi fora do ar.
O firmware emite `[OLHO]`, `[IR ]` e `[SYS ]` uma vez por segundo.

Com o monitor aberto: `s` = autoteste, `a` = arma/desarma, `m` = próximo modo,
`p` / `b` = medir piso preto / faixa branca, `?` = ajuda.

**Sempre leia o autoteste antes de suspeitar da lógica** — a maior parte dos
problemas deste projeto foi fiação, não software.

## Afinação

Os três números que mais mudam ficam no **topo do `config.h`**: `IR_LIMIAR_MV`,
`IR_BORDA_ABAIXO` e `US_ALCANCE_CM`. O painel web **não** ajusta parâmetro — ele
lê o robô e aciona os motores. A afinação é feita no código, conversando, para
que o valor que funcionou fique versionado com o comentário dizendo por quê.

## Arquitetura

Tudo roda em tasks do FreeRTOS. O C3 tem **um núcleo só**, então o que garante a
reação na borda é a **prioridade**, não a separação por núcleo:

| Prio | Task |
|---|---|
| 6 | guarda de borda — varre o AO do IR a 1 kHz |
| 4 | controle, 200 Hz |
| 2 | ultrassônico, ~16 Hz |
| 1 | Wi-Fi/HTTP e console serial |

A ordem de prioridade do comportamento é o desenho do firmware: **borda** vence
**alvo**, que vence **busca**. A borda não é um `if` dentro do laço de controle —
é uma task acima dele, que o preempta no meio da investida.

## Armadilhas já encontradas neste projeto

- **`xTaskCreatePinnedToCore(..., 1, ...)` quebra no C3.** Núcleo único: pedir o
  núcleo 1 dispara um assert do FreeRTOS no boot. Usar `xTaskCreate`.
- **`pulseIn()` é espera ocupada** e seguraria a CPU por até 38 ms por leitura do
  HC-SR04. O `ECHO` é medido por interrupção nas duas bordas, com `micros()`.
- **GPIO 2, 8 e 9 são strapping** — deixar livres. O nível deles no reset decide
  de onde a placa dá boot, e o sintoma de carregá-los não parece elétrico,
  parece firmware quebrado.
- **Sem `ARDUINO_USB_CDC_ON_BOOT` não há Serial.** O C3 SuperMini não tem chip
  USB-serial; o USB é nativo do MCU. Esse silêncio já foi confundido com
  firmware travado no boot.
- **O `EN` do L298N precisa de pull-down externo de 10 kΩ.** Entre ligar e o
  `setup()` rodar o GPIO ainda não é saída e flutua; `EN` flutuando pode
  habilitar a ponte com os `IN` indefinidos, e o robô dá um tranco ao ligar.
- **Com `EN` em ALTO o L298N não tem roda livre**: `IN1`=`IN2` freia, em ALTO ou
  em BAIXO. Soltar o motor exige baixar o `EN`.
- **A lógica do L298N (`Vss`) quer 5 V**, não 3,3 V. As entradas são outra coisa:
  pedem `Vih ≥ 2,3 V`, e os 3,3 V da C3 passam com folga.
- **A bateria não vai na ESP32.** O regulador do C3 SuperMini (ME6211) aceita
  cerca de 6,5 V; 7,8 V o queima. Step-down de 5 V, ou USB na bancada.
- **ADC2 não funciona com o Wi-Fi ligado.** O IR está no ADC1 por isso.
- **`Set-Content -Encoding utf8` no PowerShell 5.1 grava BOM**, o que quebra a
  compilação com `stray '\357'`. Escrever com
  `[System.IO.File]::WriteAllText($path, $texto, (New-Object System.Text.UTF8Encoding($false)))`.

## Convenções

Commits seguem Conventional Commits, pequenos e descritivos. Comentários e
mensagens de commit em português; identificadores de código em inglês, sem acento.

Ao instruir montagem de hardware, dar sempre **o mapa completo das ligações em
tabela** — nunca incrementos avulsos do tipo "agora acrescente este fio".
