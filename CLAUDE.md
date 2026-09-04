# Instruções do projeto — Robô Sumô (ESP32)

## O que é

Firmware de um robô de sumô de porte pequeno. ESP32 DevKit V1, 4 motores N20
6 V, 2 pontes DRV8833, 2 sensores de distância VL53L0X (os "olhos"), 2 sensores
IR de borda, OLED SSD1306 e buzzer. O painel de controle é servido pela própria
placa, num ponto de acesso Wi-Fi aberto.

## Como compilar e gravar

O toolchain é **PlatformIO**, e a placa fica ligada por USB nesta máquina.
Compilar e gravar é responsabilidade do agente, não do usuário:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run                    # compila
& $pio run --target upload    # grava (porta autodetectada)
```

A porta COM **muda** quando a placa é reconectada (já apareceu como COM3 e
COM5). Por isso `platformio.ini` não fixa a porta. Se o upload falhar com
"could not open", confira se a placa ainda está enumerada antes de suspeitar do
código:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

## Como depurar

A serial a 115200 é a fonte de verdade — ela funciona mesmo com o Wi-Fi fora do
ar. O firmware emite duas coisas por segundo:

- linhas legíveis (`[OLHO]`, `[IR ]`, `[SYS ]`)
- uma linha `#D chave=valor` para máquina, consumida por `docs/painel.ps1`

O painel de bancada mostra o estado de cada componente com verde/vermelho:

```powershell
.\docs\painel.ps1
```

Com o monitor serial aberto, uma tecla dispara teste de componente sem depender
do Wi-Fi: `t` = padrão de teste no display, `s` = repete o autoteste (e sobe o
OLED se ele acabou de ser plugado), `b` = buzzer, `a` = arma/desarma, `?` = ajuda.

No boot roda um autoteste que varre os dois barramentos I2C e imprime os
endereços encontrados. **Sempre leia o autoteste antes de suspeitar da lógica** —
a maior parte dos problemas até agora foi fiação, não software.

## Arquitetura

Tudo roda em tasks do FreeRTOS, com separação deliberada entre os núcleos:

| Núcleo | Tasks |
|---|---|
| 1 (tempo real) | guarda de borda (prio 6), controle 200 Hz (prio 4), leitura ToF (prio 2) |
| 0 (isolado) | Wi-Fi/HTTP, OLED + buzzer, bateria + botão |

O motivo: o caminho do sensor IR até o freio não pode ser atrasado por um
pedido HTTP. Não mova nada do núcleo 1 para o 0 sem entender essa restrição.

Dois barramentos I2C separados pelo mesmo motivo: OLED no `Wire` (21/22), os
VL53L0X no `Wire1` (16/17). Um quadro do OLED ocupa a linha por ~25 ms.

## Armadilhas já encontradas neste projeto

- **`F_LOCK` colide** com uma macro POSIX de `fcntl.h`. Enums de expressão facial
  usam prefixo `EX_` por causa disso.
- **GPIO 34/35/36/39 não têm pull-up interno.** Pino solto lá lê BAIXO e parece
  sinal válido.
- **GPIO 12 é strapping** — deixar livre.
- **ADC2 não funciona com Wi-Fi ligado.** Toda leitura analógica está no ADC1.
- **`Set-Content -Encoding utf8` no PowerShell 5.1 grava BOM**, o que quebra a
  compilação com `stray '\357'`. Escrever com
  `[System.IO.File]::WriteAllText($path, $texto, (New-Object System.Text.UTF8Encoding($false)))`.

## Convenções

Commits seguem Conventional Commits e o fluxo de branches está em
[CONTRIBUTING.md](CONTRIBUTING.md). Comentários e mensagens de commit em
português; identificadores de código em inglês, sem acento.
