# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Build helper (xgHub.CoreCLR XGEHubShortcut target): writes <BinDir>\xgHub.lnk, which opens the Project Hub through
# conhost.exe --headless so no console window appears. Rewrites the shortcut only when it is missing or differs.
param([Parameter(Mandatory = $true)][string]$BinDir)

$ErrorActionPreference = "Stop"
$BinDir = (Resolve-Path $BinDir).Path
$link = Join-Path $BinDir "xgHub.lnk"
$target = Join-Path $env:SystemRoot "System32\conhost.exe"
$arguments = "--headless `"$(Join-Path $BinDir 'xge.exe')`" --config hub.config"
$icon = (Resolve-Path (Join-Path $BinDir "..\editor\xge-icon.ico") -ErrorAction SilentlyContinue)
$iconLocation = if ($icon) { "$($icon.Path),0" } else { "$target,0" }

$shell = New-Object -ComObject WScript.Shell
if (Test-Path $link) {
    $existing = $shell.CreateShortcut($link)
    if ($existing.TargetPath -ieq $target -and $existing.Arguments -eq $arguments -and
        $existing.WorkingDirectory -ieq $BinDir -and $existing.IconLocation -ieq $iconLocation) {
        exit 0
    }
}
$shortcut = $shell.CreateShortcut($link)
$shortcut.TargetPath = $target
$shortcut.Arguments = $arguments
$shortcut.WorkingDirectory = $BinDir
$shortcut.IconLocation = $iconLocation
$shortcut.Description = "XGE Project Hub"
$shortcut.Save()
Write-Host "xgHub shortcut: $link"
