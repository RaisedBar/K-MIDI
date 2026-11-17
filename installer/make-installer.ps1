# Builds the Windows installer and portable zip, in build/windows-vs2022/installer:
#   K-MIDI-<version>-setup.exe
#   K-MIDI-<version>-portable.zip
#
# Needs everything listed under Building in README.md, plus Inno Setup 6
# (winget install JRSoftware.InnoSetup).
#
# It configures, builds Release, runs the tests, puts the program and its DLLs in build/windows-vs2022/stage,
# compiles the Inno Setup script CMake made from installer/K-MIDI.iss.in, then zips the staged files.

$ErrorActionPreference = 'Stop'
$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build\windows-vs2022'
$stage = Join-Path $build 'stage'

function Invoke-Step([string]$what, [scriptblock]$command) {
    Write-Host "== $what"
    & $command
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}

# Find the Inno Setup compiler: on the PATH, or where a per-user or all-users install puts it
$iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { throw 'Inno Setup 6 was not found. Install it with: winget install JRSoftware.InnoSetup' }

Push-Location $root
try {
    Invoke-Step 'Configure'  { cmake --preset windows-vs2022 }
    Invoke-Step 'Build'      { cmake --build --preset windows-release }
    Invoke-Step 'Test'       { ctest --test-dir $build -C Release --output-on-failure }

    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    Invoke-Step 'Stage'      { cmake --install $build --config Release --prefix $stage }
    Invoke-Step 'Installer'  { & $iscc /Q (Join-Path $build 'K-MIDI.iss') }

    # The portable version is the same files in a zip, inside a K-MIDI-<version> folder. K-MIDI keeps no settings
    # and writes nothing to the registry, so it runs from wherever it's unzipped (a USB drive, for example).
    Write-Host '== Portable zip'
    $version = (Select-String -Path (Join-Path $build 'K-MIDI.iss') -Pattern '^#define AppVersion\s+"(.+)"').Matches[0].Groups[1].Value
    $portable = Join-Path $build "portable\K-MIDI-$version"
    $zip = Join-Path $build "installer\K-MIDI-$version-portable.zip"
    if (Test-Path (Split-Path $portable)) { Remove-Item -Recurse -Force (Split-Path $portable) }
    Copy-Item -Recurse $stage $portable
    Compress-Archive -Path $portable -DestinationPath $zip -Force
}
finally {
    Pop-Location
}

Get-ChildItem (Join-Path $build 'installer\*') -Include *.exe, *.zip | ForEach-Object { Write-Host "Made $($_.FullName)" }
