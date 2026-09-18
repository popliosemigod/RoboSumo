# Instruções do projeto — Robô Sumô (ESP32-C3)

## O que é

Firmware de um robô de sumô autônomo da categoria Mini Sumô. **ESP32-C3 Mini**,
4 motores DC, 2 pontes DRV8833, 1 HC-SR04 (o "olho"), 1 sensor IR de borda e o
receptor IR do juiz. O painel de controle é servido pela própria placa, num
ponto de acesso Wi-Fi aberto.

A pinagem oficial e a lista de material estão no [README.md](README.md); o
esquemático em [docs/schematic.svg](docs/schematic.svg). **Só entra nesses
documentos hardware que está fisicamente montado** — sonda de diagnóstico e
circuito planejado não aparecem como se existissem.

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

Com o monitor aberto: `s` = autoteste, `a` = arma/desarma, `?` = ajuda.

**Sempre leia o autoteste antes de suspeitar da lógica** — a maior parte dos
problemas deste projeto foi fiação, não software.

## Arquitetura

Tudo roda em tasks do FreeRTOS. O C3 tem **um núcleo só**, então o que garante a
reação na borda é a **prioridade**, não a separação por núcleo:

| Prio | Task |
|---|---|
| 6 | guarda de borda — acordada pela ISR do IR |
| 4 | controle, 200 Hz |
| 2 | ultrassônico, ~16 Hz |
| 1 | Wi-Fi/HTTP, juiz e console serial |

O caminho do sensor IR até o freio não pode ser atrasado por um pedido HTTP. O
FreeRTOS é preemptivo e a prioridade 6 tira o HTTP da CPU no meio de um pedido —
mas isso só vale enquanto ninguém baixar essa prioridade nem bloquear a CPU sem
ceder.

## Armadilhas já encontradas neste projeto

- **`xTaskCreatePinnedToCore(..., 1, ...)` quebra no C3.** Núcleo único: pedir o
  núcleo 1 dispara um assert do FreeRTOS no boot. Usar `xTaskCreate`.
- **`pulseIn()` é espera ocupada** e seguraria a CPU por até 38 ms por leitura do
  HC-SR04. O `ECHO` é medido por interrupção nas duas bordas, com `micros()`.
- **GPIO 2, 8 e 9 são strapping** — deixar livres. O nível deles no reset decide
  de onde a placa dá boot, e o sintoma de carregá-los não parece elétrico,
  parece firmware quebrado.
- **Sem `ARDUINO_USB_CDC_ON_BOOT` não há Serial.** O C3 Mini não tem chip
  USB-serial; o USB é nativo do MCU. Esse silêncio já foi confundido com
  firmware travado no boot.
- **Pino de sensor sem pull-up mente.** Fio solto repousa em BAIXO, que é
  exatamente o nível de "borda" — jumper mal encaixado paralisa o robô e se
  disfarça de leitura legítima.
- **`Set-Content -Encoding utf8` no PowerShell 5.1 grava BOM**, o que quebra a
  compilação com `stray '\357'`. Escrever com
  `[System.IO.File]::WriteAllText($path, $texto, (New-Object System.Text.UTF8Encoding($false)))`.

## Convenções

Commits seguem Conventional Commits. Comentários e mensagens de commit em
português; identificadores de código em inglês, sem acento.

Ao instruir montagem de hardware, dar sempre **o mapa completo das ligações em
tabela** — nunca incrementos avulsos do tipo "agora acrescente este fio".
