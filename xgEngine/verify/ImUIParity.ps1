# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Checks that every XG_ImUI* export of xgImUIKit.dll is bound in xgImUIKit.Managed (LibraryImport EntryPoint)
# and wrapped in the native C++ layer (xgImUIGui.hpp). Host-only exports (XG_ImUIHost*) are exempt.
# Usage: powershell -File verify\ImUIParity.ps1 [-Dll ..\gameroot\bin\xgImUIKit.dll]
param([string]$Dll = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $Dll) { $Dll = Join-Path $root "..\gameroot\bin\xgImUIKit.dll" }

$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vs = & "$installer\vswhere.exe" -latest -property installationPath
$dumpbin = Get-ChildItem "$vs\VC\Tools\MSVC" -Recurse -Filter dumpbin.exe | Where-Object { $_.FullName -match "Hostx64\\x64" } | Select-Object -First 1
$exports = & $dumpbin.FullName /nologo /exports $Dll |
    Select-String -Pattern "^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(XG_ImUI\w+)" |
    ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Where-Object { $_ -notlike "XG_ImUIHost*" } | Sort-Object -Unique

$managedSource = Get-Content (Join-Path $root "xgImUIKit.Managed\Native.cs") -Raw
$managed = [regex]::Matches($managedSource, 'EntryPoint\s*=\s*"(XG_ImUI\w+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$nativeSource = Get-Content (Join-Path $root "xgImUIKit\public\xgImUIGui.hpp") -Raw
$native = [regex]::Matches($nativeSource, '\b(XG_ImUI\w+)\s*\(') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$missingManaged = $exports | Where-Object { $managed -notcontains $_ }
$missingNative = $exports | Where-Object { $native -notcontains $_ -and $_ -ne "XG_ImUIGetApiVersion" }
$staleManaged = $managed | Where-Object { $exports -notcontains $_ }

"exports: $($exports.Count), managed bindings: $($managed.Count), native wrapper calls: $($native.Count)"
if ($missingManaged) { "missing in xgImUIKit.Managed: $($missingManaged -join ', ')" }
if ($missingNative) { "missing in xgImUIGui.hpp: $($missingNative -join ', ')" }
if ($staleManaged) { "bound in managed but not exported: $($staleManaged -join ', ')" }
if ($missingManaged -or $missingNative -or $staleManaged) { exit 1 }
"parity OK"
