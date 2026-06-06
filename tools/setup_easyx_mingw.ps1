param(
    [string]$InstallDir = "third_party\easyx4mingw",
    [string]$DownloadUrl = "https://easyx.cn/download/easyx4mingw_25.9.10.zip"
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path ".").Path
$target = Join-Path $root $InstallDir
$raw = Join-Path $target "_raw"
$tmp = Join-Path $env:TEMP "easyx4mingw.zip"
$includeTarget = Join-Path $target "include"
$lib64Target = Join-Path $target "lib64"

New-Item -ItemType Directory -Force -Path $target | Out-Null

$graphicsHeader = Join-Path $includeTarget "graphics.h"
$easyxLib = Join-Path $lib64Target "libeasyx.a"

if ((Test-Path $graphicsHeader) -and (Test-Path $easyxLib)) {
    Write-Host "EasyX for MinGW is already ready in $target"
    exit 0
}

if (Test-Path $raw) {
    Remove-Item -Recurse -Force $raw
}

New-Item -ItemType Directory -Force -Path $raw | Out-Null
New-Item -ItemType Directory -Force -Path $includeTarget | Out-Null
New-Item -ItemType Directory -Force -Path $lib64Target | Out-Null

Write-Host "Downloading EasyX for MinGW from $DownloadUrl ..."
Invoke-WebRequest -Uri $DownloadUrl -OutFile $tmp

Write-Host "Extracting to $raw..."
Expand-Archive -Path $tmp -DestinationPath $raw -Force

$graphicsSource = Get-ChildItem -Path $raw -Recurse -Filter "graphics.h" | Select-Object -First 1
$easyxSource = Get-ChildItem -Path $raw -Recurse -Filter "libeasyx.a" |
    Sort-Object @{ Expression = { if ($_.FullName -match "64|x64") { 0 } else { 1 } } }, FullName |
    Select-Object -First 1

if ($null -eq $graphicsSource) {
    throw "graphics.h was not found in the EasyX archive."
}

if ($null -eq $easyxSource) {
    throw "libeasyx.a was not found in the EasyX archive."
}

Copy-Item -Force $graphicsSource.FullName $graphicsHeader
Copy-Item -Force $easyxSource.FullName $easyxLib

$easyxHeaderSource = Get-ChildItem -Path $raw -Recurse -Filter "easyx.h" | Select-Object -First 1
if ($null -ne $easyxHeaderSource) {
    Copy-Item -Force $easyxHeaderSource.FullName (Join-Path $includeTarget "easyx.h")
}

Write-Host "EasyX for MinGW is ready in $target"
