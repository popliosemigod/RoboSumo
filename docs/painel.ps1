# =====================================================================
#  ROBO SUMO - painel de bancada (PC)
#
#  Le a serial do ESP32 e mostra, ao vivo, o estado de cada componente
#  ligado na placa. Nao precisa do Wi-Fi nem do painel web: serve
#  justamente para achar problema de fiacao antes de tudo isso subir.
#
#  Uso:   .\docs\painel.ps1              (porta COM3, padrao)
#         .\docs\painel.ps1 -Porta COM5
#         .\docs\painel.ps1 -SemReset    (nao reinicia a placa ao abrir)
#
#  Sair: Ctrl+C
# =====================================================================
param(
    [string] $Porta    = "COM3",
    [int]    $Baud     = 115200,
    [switch] $SemReset,
    [int]    $Segundos = 0        # 0 = roda ate Ctrl+C
)

$ErrorActionPreference = "Stop"

# ---- estado corrente, alimentado pelas linhas "#D" do firmware -------
$D = @{}
$boot = New-Object System.Collections.Generic.List[string]
$ultimoDado = $null

$ESTADOS = @("IDLE","CONTAGEM","BUSCA","TRAVANDO","ATAQUE","BORDA!","DESTRAVE","DANCA","MANUAL","FALHA")
$MODOS   = @("NORMAL","LESMA","CAPIROTO","CACADOR","MURALHA","DANCINHA")

function Num($chave, $padrao = 0) {
    if ($D.ContainsKey($chave)) { return [int]$D[$chave] }
    return $padrao
}

function Linha($rotulo, $ok, $detalhe) {
    $marca = if ($ok -eq $true) { "[ OK ]" } elseif ($ok -eq $false) { "[FALHA]" } else { "[ -- ]" }
    $cor   = if ($ok -eq $true) { "Green" }  elseif ($ok -eq $false) { "Red" }     else { "DarkGray" }
    Write-Host ("  {0,-7}" -f $marca) -ForegroundColor $cor -NoNewline
    Write-Host ("{0,-26}" -f $rotulo) -NoNewline
    Write-Host $detalhe -ForegroundColor DarkGray
}

function Barra($cm, $max) {
    if ($cm -lt 0) { return "".PadRight(24, '.') }
    $n = [Math]::Min(24, [int](24 * (1 - [Math]::Min(1.0, $cm / [double]$max))))
    return ("#" * $n).PadRight(24, '.')
}

# Console de verdade permite redesenhar por cima; saida redirecionada
# (pipe, arquivo, CI) nao permite - nesse caso o painel so vai rolando.
$script:temConsole = $true
try { $null = [Console]::CursorTop } catch { $script:temConsole = $false }

function Desenha {
    $agora = Get-Date
    $vivo  = $ultimoDado -and (($agora - $ultimoDado).TotalSeconds -lt 3)

    if ($script:temConsole) {
        try { [Console]::SetCursorPosition(0, 0) } catch { $script:temConsole = $false }
    }
    if (-not $script:temConsole) { Write-Host "" }
    Write-Host "===============================================================" -ForegroundColor DarkCyan
    Write-Host "  ROBO SUMO - painel de bancada          " -NoNewline -ForegroundColor Cyan
    if ($vivo) { Write-Host "porta $Porta  RECEBENDO " -ForegroundColor Green }
    else       { Write-Host "porta $Porta  SEM DADOS " -ForegroundColor Red }
    Write-Host "===============================================================" -ForegroundColor DarkCyan
    Write-Host ""

    # ---------------- controlador ----------------
    Write-Host "  CONTROLADOR" -ForegroundColor Yellow
    $hz = Num "hz"
    Linha "ESP32 / malha de controle" ($hz -gt 150) "$hz Hz   heap livre $(Num 'heap') bytes"
    $st = Num "st"; $md = Num "mode"
    $nomeSt = if ($st -lt $ESTADOS.Count) { $ESTADOS[$st] } else { "?" }
    $nomeMd = if ($md -lt $MODOS.Count)   { $MODOS[$md] }   else { "?" }
    Linha "Estado / modo" $null "$nomeSt   modo $nomeMd   $(if (Num 'armed') {'ARMADO'} else {'desarmado'})"
    Linha "Wi-Fi ROBO_SUMO (192.168.4.1)" $true "$(Num 'ap') aparelho(s) conectado(s)"
    Linha "Display OLED SSD1306 (0x3C)" ((Num "oled") -eq 1) "barramento I2C 21/22"
    Write-Host ""

    # ---------------- olhos ----------------
    Write-Host "  OLHOS - VL53L0X (Time-of-Flight, I2C 16/17)" -ForegroundColor Yellow
    foreach ($lado in @(
        @{ n = "Olho ESQUERDO (0x30)"; ok = "tofL"; d = "dL"; r = "rL"; f = "fL"; x = "XSHUT GPIO19" },
        @{ n = "Olho DIREITO  (0x29)"; ok = "tofR"; d = "dR"; r = "rR"; f = "fR"; x = "XSHUT GPIO18" }
    )) {
        $presente = (Num $lado.ok) -eq 1
        if (-not $presente) {
            if ((Num "bus") -eq 1) {
                Linha $lado.n $false "algo responde no barramento mas este nao subiu - conferir $($lado.x)"
            } else {
                Linha $lado.n $false "barramento VAZIO - conferir VIN 3V3, GND, SDA=16, SCL=17"
            }
            continue
        }
        $cm = Num $lado.d (-1)
        $mm = Num $lado.r (-1)
        $txt = if ($cm -ge 0) { "$cm cm  (cru $mm mm)" } else { "sem alvo no alcance  (falhas seguidas: $(Num $lado.f))" }
        Linha $lado.n $true $txt
        Write-Host ("           " + (Barra $cm 80)) -ForegroundColor $(if ($cm -ge 0) { "Cyan" } else { "DarkGray" })
    }
    Write-Host ""

    # ---------------- borda ----------------
    Write-Host "  SENSORES DE BORDA - IR" -ForegroundColor Yellow
    foreach ($lado in @(
        @{ n = "IR ESQUERDO (GPIO34/32)"; d = "irLd"; a = "irLa"; v = "irL" },
        @{ n = "IR DIREITO  (GPIO35/33)"; d = "irRd"; a = "irRa"; v = "irR" }
    )) {
        $adc = Num $lado.a
        $pino = if ((Num $lado.d) -eq 1) { "ALTO" } else { "BAIXO" }
        # ADC exatamente 0 com pino em BAIXO = modulo sem alimentacao
        $morto = ($adc -eq 0 -and (Num $lado.d) -eq 0)
        $veBorda = (Num $lado.v) -eq 1
        if ($morto) {
            Linha $lado.n $false "sem sinal: pino BAIXO e ADC 0 -> modulo provavelmente sem VCC/GND"
        } else {
            Linha $lado.n $true "pino $pino   ADC $adc   -> $(if ($veBorda) {'BORDA'} else {'zona segura'})"
        }
    }
    Write-Host ""

    # ---------------- potencia ----------------
    Write-Host "  POTENCIA" -ForegroundColor Yellow
    $vb = (Num "vbat") / 100.0
    $vbOk = $vb -ge 6.0
    Linha "Bateria (divisor GPIO39)" $(if ($vb -lt 0.9) { $false } else { $vbOk }) `
        $("{0:N2} V" -f $vb + $(if ($vb -lt 0.9) { "   divisor parece desligado" } else { "" }))
    Linha "DRV8833 nFAULT (GPIO15)" ((Num "drv") -eq 0) $(if ((Num "drv") -eq 1) { "FALHA sinalizada pela ponte" } else { "sem falha" })
    Linha "PWM aplicado (esq / dir)" $null "$(Num 'pwmL') / $(Num 'pwmR')  (escala -1000..1000)"
    Write-Host ""

    Write-Host "  Ctrl+C para sair" -ForegroundColor DarkGray
    Write-Host "                                                               "
}

# ---------------------------------------------------------------------
$sp = New-Object System.IO.Ports.SerialPort($Porta, $Baud, "None", 8, "One")
$sp.ReadTimeout  = 200
$sp.DtrEnable    = $false
$sp.RtsEnable    = $false

try {
    $sp.Open()
} catch {
    Write-Host "Nao consegui abrir $Porta." -ForegroundColor Red
    Write-Host "Feche o Monitor Serial da IDE / o 'pio device monitor' e tente de novo." -ForegroundColor Yellow
    exit 1
}

if (-not $SemReset) {
    # pulso no EN para pegar o banner de boot e o autoteste
    $sp.DtrEnable = $true; $sp.RtsEnable = $true
    Start-Sleep -Milliseconds 120
    $sp.DtrEnable = $false; $sp.RtsEnable = $false
}

if ($script:temConsole) { Clear-Host }
try { [Console]::CursorVisible = $false } catch {}

$buffer = ""
$inicio = Get-Date
try {
    while ($Segundos -le 0 -or ((Get-Date) - $inicio).TotalSeconds -lt $Segundos) {
        try { $buffer += $sp.ReadExisting() } catch {}

        while ($buffer.Contains("`n")) {
            $i = $buffer.IndexOf("`n")
            $linha = $buffer.Substring(0, $i).Trim("`r", " ")
            $buffer = $buffer.Substring($i + 1)

            if ($linha.StartsWith("#D ")) {
                foreach ($par in $linha.Substring(3).Split(" ")) {
                    $kv = $par.Split("=")
                    if ($kv.Count -eq 2) { $D[$kv[0]] = $kv[1] }
                }
                $ultimoDado = Get-Date
            }
            elseif ($linha -match "^\[(tof|teste|boot|nvs|web)\]") {
                $boot.Add($linha)
                if ($boot.Count -gt 12) { $boot.RemoveAt(0) }
            }
        }

        Desenha
        # sem console para redesenhar por cima, imprimir 4x/s viraria spam
        Start-Sleep -Milliseconds $(if ($script:temConsole) { 250 } else { 2000 })
    }
}
finally {
    try { [Console]::CursorVisible = $true } catch {}
    if ($sp.IsOpen) { $sp.Close() }
    Write-Host ""
    Write-Host "Painel encerrado. Ultimas linhas de boot da placa:" -ForegroundColor DarkCyan
    $boot | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
}
