$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDir

if ($env:JUCE_PATH -and (Test-Path (Join-Path $env:JUCE_PATH "CMakeLists.txt"))) {
    $JucePath = $env:JUCE_PATH
}
elseif (Test-Path (Join-Path $HOME "JUCE\CMakeLists.txt")) {
    $JucePath = Join-Path $HOME "JUCE"
}
elseif (Test-Path (Join-Path $HOME "Nextcloud2\Projekte\Development\JUCE\Framework\CMakeLists.txt")) {
    $JucePath = Join-Path $HOME "Nextcloud2\Projekte\Development\JUCE\Framework"
}
else {
    Write-Host "ERROR: JUCE not found."
    Write-Host 'Set it first, e.g.: $env:JUCE_PATH = "C:\path\to\JUCE"'
    exit 1
}

$BuildDir = Join-Path $ProjectDir "build_win"

Write-Host "SignalAlign Windows Build"
Write-Host "Project: $ProjectDir"
Write-Host "JUCE:    $JucePath"
Write-Host "Build:   $BuildDir"

cmake -S $ProjectDir -B $BuildDir -G "Visual Studio 17 2022" -A x64 "-DJUCE_PATH=$JucePath"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDir --config Release --target SignalAlign_VST3 SignalAlign_Standalone SignalAlign16_VST3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Windows build complete."
Write-Host "Standalone: $BuildDir\SignalAlign_artefacts\Release\Standalone\SignalAlign.exe"
Write-Host "VST3:       $BuildDir\SignalAlign_artefacts\Release\VST3\SignalAlign.vst3"
Write-Host "16 VST3:    $BuildDir\SignalAlign16_artefacts\Release\VST3\SignalAlign 16.vst3"
