#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Qt Android arm64-v8a Release APK build script

.PARAMETER Clean
    Clean build cache, reconfigure and rebuild.

.EXAMPLE
    .\build_arm64-v8a_apk.ps1
    Incremental build and generate APK

.EXAMPLE
    .\build_arm64-v8a_apk.ps1 -Clean
    Clean rebuild
#>

[CmdletBinding()]
param(
    [Parameter(HelpMessage = "Clean build cache")]
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# === Config ===
$env:PATH = "C:\Qt\6.11.1\llvm-mingw_64\bin;C:\Qt\Android\cmdline-tools\latest\bin;$env:PATH"

$SOURCE_DIR = "D:\AwaCode\qtmultimedia-f"
$BUILD_DIR = "$SOURCE_DIR\build-release-arm64-v8a"
$DEPLOY_OUTPUT = "$BUILD_DIR\bin\android-build-qzPlayerExample"
$ANDROID_PLATFORM = "android-36"
$ANDROID_MIN_SDK = 28
$JDK_PATH = "C:/Program Files/Java/jdk-17"

$KEYSTORE_PATH = "D:\AwaCode\android_release.keystore"
$KEY_ALIAS = "awa"
$STORE_PASS = "123123"
# =========================

Write-Host "=== Qt Android arm64-v8a Release APK Build ===" -ForegroundColor Cyan

# === Clean ===
if ($Clean -and (Test-Path $BUILD_DIR)) {
    Write-Host "`nCleaning build dir: $BUILD_DIR" -ForegroundColor Yellow

    $gradlewPath = Join-Path $DEPLOY_OUTPUT "gradlew.bat"
    if (Test-Path $gradlewPath) {
        Push-Location (Split-Path $gradlewPath -Parent)
        & .\gradlew.bat --stop 2>$null
        Pop-Location
    }

    Remove-Item -Path $BUILD_DIR -Recurse -Force -ErrorAction SilentlyContinue
}

# === Step 1: CMake configure ===
if (!(Test-Path "$BUILD_DIR\build.ninja") -or !(Test-Path "$BUILD_DIR\CMakeCache.txt")) {
    Write-Host "`n[1/3] CMake configure..." -ForegroundColor Cyan
    & cmake.exe -DCMAKE_BUILD_TYPE=Release --preset android-arm64-release -S $SOURCE_DIR -B $BUILD_DIR
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed!" -ForegroundColor Red; exit 1 }
    Write-Host "CMake configure done" -ForegroundColor Green
} else {
    Write-Host "`n[1/3] Skip CMake configure (incremental)" -ForegroundColor Yellow
}

# === Step 2: Build ===
Write-Host "`n[2/3] Building..." -ForegroundColor Cyan
& cmake.exe --build $BUILD_DIR --target all -j 8
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed!" -ForegroundColor Red; exit 1 }
Write-Host "Build done" -ForegroundColor Green

# === Step 3: androiddeployqt ===
Write-Host "`n[3/3] Generating APK..." -ForegroundColor Cyan

$DEPLOYMENT_SETTINGS = "$BUILD_DIR\bin\android-qzPlayerExample-deployment-settings.json"
if (!(Test-Path $DEPLOYMENT_SETTINGS)) {
    Write-Host "deployment-settings.json not found" -ForegroundColor Red; exit 1
}

# Stop old Gradle daemon
$gradlewPath = Join-Path $DEPLOY_OUTPUT "gradlew.bat"
if (Test-Path $gradlewPath) {
    Push-Location (Split-Path $gradlewPath -Parent)
    & .\gradlew.bat --stop 2>$null
    Pop-Location
    Start-Sleep -Seconds 1
}

& "C:\Qt\6.11.1\mingw_64\bin\androiddeployqt.exe" `
    --input $DEPLOYMENT_SETTINGS `
    --output $DEPLOY_OUTPUT `
    --android-platform $ANDROID_PLATFORM `
    --jdk $JDK_PATH `
    --gradle `
    --release `
    --sign $KEYSTORE_PATH $KEY_ALIAS `
    --storepass $STORE_PASS `
    --no-gdbserver `
    --min-sdk-version $ANDROID_MIN_SDK

if ($LASTEXITCODE -ne 0) { Write-Host "androiddeployqt failed!" -ForegroundColor Red; exit 1 }
Write-Host "APK generated" -ForegroundColor Green

# === Open APK directory ===
$APK_DIR = "$DEPLOY_OUTPUT\build\outputs\apk\release"
if (Test-Path $APK_DIR) {
    $apk = Get-ChildItem $APK_DIR -Filter "*.apk" | Select-Object -First 1
    if ($apk) {
        $sizeMB = [math]::Round($apk.Length / 1MB, 2)
        Write-Host "`nAPK: $($apk.Name) ($sizeMB MB)" -ForegroundColor Green
    }
    explorer $APK_DIR
} else {
    Write-Host "APK dir not found: $APK_DIR" -ForegroundColor Red
}
