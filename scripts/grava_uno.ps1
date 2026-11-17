param(
    [Parameter(Mandatory=$true)]
    [string]$Hex,

    [Parameter(Mandatory=$true)]
    [string]$Port
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw "arduino-cli nao encontrado no PATH. Instale o Arduino CLI oficial para Windows."
}

$resolved = (Resolve-Path $Hex).Path

$coreList = arduino-cli core list
if ($LASTEXITCODE -ne 0) {
    throw "Falha ao consultar cores do Arduino CLI."
}

if (-not ($coreList -match "arduino:avr")) {
    Write-Host "Instalando core arduino:avr..."
    arduino-cli core update-index
    if ($LASTEXITCODE -ne 0) { throw "core update-index falhou." }
    arduino-cli core install arduino:avr
    if ($LASTEXITCODE -ne 0) { throw "core install arduino:avr falhou." }
}

Write-Host "Gravando $resolved em $Port..."
arduino-cli upload -p $Port -b arduino:avr:uno -i $resolved -v
if ($LASTEXITCODE -ne 0) {
    throw "Upload para o Arduino Uno falhou."
}

Write-Host "Upload concluido. Abra o monitor em 9600 baud e pressione RESET para repetir a execucao."
