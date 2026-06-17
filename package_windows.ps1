param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppName = "qzPlayerExample"
$AppVersion = "0.1.0"
$BuildDir = "$ScriptDir\build-release"
$BinDir = "$BuildDir\bin"
$PackageDir = "$ScriptDir\package"
$QtPath = "C:\Qt\6.11.1\llvm-mingw_64"
$FFmpegDir = "$ScriptDir\multimedia\src\3rdparty\ffmpeg\ffmpeg_win64_8.0.2"
$MinGWBin = "C:\AwaLib\llvm-mingw-20251216-ucrt-x86_64\bin"

function Write-Step($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "  [OK] $msg" -ForegroundColor Green }

Write-Step "1/2 Building Release"
if (-not $SkipBuild) {
    cmake.exe --build --target all --preset llvm-mingw-release -j ([Environment]::ProcessorCount)
    if ($LASTEXITCODE -ne 0) { Write-Host "  [FAIL] Build failed" -ForegroundColor Red; exit 1 }
    Write-Ok "Build succeeded"
} else {
    Write-Ok "Skipped (--SkipBuild)"
}

if (-not (Test-Path "$BinDir\$AppName.exe")) {
    Write-Host "  [FAIL] $AppName.exe not found in $BinDir" -ForegroundColor Red; exit 1
}

Write-Step "2/2 Collecting dependencies"
$DeployDir = "$PackageDir\$AppName"
if (Test-Path $DeployDir) { Remove-Item $DeployDir -Recurse -Force }
New-Item -ItemType Directory -Path $DeployDir -Force | Out-Null

Write-Host "  Copying executable and DLLs from build..." -ForegroundColor White
Get-ChildItem "$BinDir\*.exe" | Copy-Item -Destination $DeployDir -Force
Get-ChildItem "$BinDir\*.dll" | Copy-Item -Destination $DeployDir -Force
Write-Ok "Copied $(Get-ChildItem "$DeployDir\*.exe","$DeployDir\*.dll" | Measure-Object).Count files"

Write-Host "  Copying Qt runtime DLLs..." -ForegroundColor White
$QtDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Network.dll", "Qt6Qml.dll", "Qt6QmlMeta.dll", "Qt6QmlModels.dll", "Qt6Quick.dll", "Qt6QuickControls2.dll", "Qt6QuickLayouts.dll", "Qt6Concurrent.dll")
foreach ($dll in $QtDlls) {
    $src = "$QtPath\bin\$dll"
    if (Test-Path $src) { Copy-Item $src "$DeployDir\" -Force }
}
Write-Ok "Copied Qt runtime DLLs"

Write-Host "  Copying Qt plugins..." -ForegroundColor White
$PluginTypes = @("platforms", "styles", "imageformats", "iconengines", "tls")
foreach ($pt in $PluginTypes) {
    $src = "$QtPath\plugins\$pt"
    if (Test-Path $src) { Copy-Item $src "$DeployDir\plugins\$pt" -Recurse -Force }
}
Write-Ok "Copied Qt plugins"

Write-Host "  Copying QML modules..." -ForegroundColor White
# Base QML modules (small, needed for Qt object and QtQml base)
$QmlBaseModules = @("Qt", "QtQml")
foreach ($mod in $QmlBaseModules) {
    $src = "$QtPath\qml\$mod"
    if (Test-Path $src) { Copy-Item $src "$DeployDir\qml\$mod" -Recurse -Force }
}
# QtQuick - only copy needed submodules instead of entire directory
New-Item -ItemType Directory -Path "$DeployDir\qml\QtQuick" -Force | Out-Null
Get-ChildItem "$QtPath\qml\QtQuick" -File | Copy-Item -Destination "$DeployDir\qml\QtQuick" -Force
$QtQuickSubs = @("Controls", "Layouts", "Dialogs", "Window", "Templates")
foreach ($sub in $QtQuickSubs) {
    $src = "$QtPath\qml\QtQuick\$sub"
    if (Test-Path $src) { Copy-Item $src "$DeployDir\qml\QtQuick\$sub" -Recurse -Force }
}
Write-Ok "Copied QML modules"

Write-Host "  Copying custom QML modules (qz/)..." -ForegroundColor White
$CustomQmlSrc = "$BinDir\qz"
if (Test-Path $CustomQmlSrc) {
    Copy-Item $CustomQmlSrc "$DeployDir\qml\qz" -Recurse -Force
    Write-Ok "Copied custom QML modules"
} else {
    Write-Host "  [WARN] Custom QML modules not found at $CustomQmlSrc" -ForegroundColor Yellow
}

Write-Host "  Copying multimedia plugins..." -ForegroundColor White
$MediaPluginSrc = "$BinDir\plugins\multimedia"
if (Test-Path $MediaPluginSrc) {
    New-Item -ItemType Directory -Path "$DeployDir\plugins\multimedia" -Force | Out-Null
    Copy-Item "$MediaPluginSrc\*" "$DeployDir\plugins\multimedia\" -Recurse -Force
    Write-Ok "Copied multimedia plugins"
} else {
    Write-Host "  [WARN] Multimedia plugins not found at $MediaPluginSrc" -ForegroundColor Yellow
}

Write-Host "  Creating qt.conf..." -ForegroundColor White
$QtConfContent = "[Paths]`nPrefix = .`nPlugins = plugins`nQml2Imports = qml`n"
[System.IO.File]::WriteAllText("$DeployDir\qt.conf", $QtConfContent, [System.Text.UTF8Encoding]::new($false))
Write-Ok "Created qt.conf"

Write-Host "  Cleaning unused files..." -ForegroundColor White
$UnusedStyles = @("FluentWinUI3", "Fusion", "Universal", "Imagine", "Windows")
foreach ($style in $UnusedStyles) {
    $p = "$DeployDir\qml\QtQuick\Controls\$style"
    if (Test-Path $p) { Remove-Item $p -Recurse -Force }
}
$UnusedDlls = @("opengl32sw.dll", "d3dcompiler_47.dll", "Qt6Designer.dll", "Qt6ShaderTools.dll", "Qt6Widgets.dll", "Qt6QuickControls2Imagine.dll", "Qt6OpenGL.dll", "Qt6QmlWorkerScript.dll", "Qt6QuickEffects.dll", "Qt6QuickShapes.dll")
foreach ($dll in $UnusedDlls) {
    $p = "$DeployDir\$dll"
    if (Test-Path $p) { Remove-Item $p -Force }
}
# Remove build artifacts and debug files
foreach ($ext in @("*.pdb", "*.prl", "*.lib", "*.a", "*.cmake", "*_debug.*", "*.qrc")) {
    Get-ChildItem $DeployDir -Recurse -Filter $ext | Remove-Item -Force
}
# Remove unused platform plugins (only qwindows.dll is needed for desktop)
foreach ($plat in @("qoffscreen.dll", "qminimal.dll", "qdirect2d.dll")) {
    $p = "$DeployDir\plugins\platforms\$plat"
    if (Test-Path $p) { Remove-Item $p -Force }
}
# Remove unused imageformat plugins (keep jpeg, svg, gif, ico, webp)
$KeepImageFormats = @("qjpeg.dll", "qsvg.dll", "qgif.dll", "qico.dll", "qwebp.dll")
Get-ChildItem "$DeployDir\plugins\imageformats" -Filter "*.dll" -ErrorAction SilentlyContinue | Where-Object { $KeepImageFormats -notcontains $_.Name } | Remove-Item -Force
# Remove .qml source files from Qt's own QML modules (compiled into plugin DLLs)
# Keep .qml files in qz/ (custom modules are not compiled into DLLs)
Get-ChildItem "$DeployDir\qml\Qt","$DeployDir\qml\QtQml","$DeployDir\qml\QtQuick" -Recurse -Filter "*.qml" -ErrorAction SilentlyContinue | Remove-Item -Force
Write-Ok "Removed .qml source files from Qt QML modules"

$cleaned = (Get-ChildItem $DeployDir -Recurse -File | Measure-Object).Count
Write-Ok "Cleaned: $cleaned files"

Write-Host "  Copying extra DLLs..." -ForegroundColor White
$ExtraDlls = @(
    @{ Src = "$MinGWBin\libc++.dll"; Desc = "libc++.dll" },
    @{ Src = "$MinGWBin\libunwind.dll"; Desc = "libunwind.dll" }
)
$FFmpegLibs = Get-ChildItem -Path "$FFmpegDir\bin" -Filter "*.dll" -ErrorAction SilentlyContinue
foreach ($dll in $FFmpegLibs) {
    $ExtraDlls += @{ Src = $dll.FullName; Desc = $dll.Name }
}

$Copied = 0
foreach ($item in $ExtraDlls) {
    if (Test-Path $item.Src) {
        Copy-Item $item.Src "$DeployDir\" -Force
        Write-Ok "Copied $($item.Desc)"
        $Copied++
    } else {
        Write-Host "  [WARN] $($item.Desc) not found" -ForegroundColor Yellow
    }
}
Write-Ok "Copied $Copied extra DLLs"

$deployFinal = (Get-ChildItem $DeployDir -Recurse -File | Measure-Object).Count
Write-Ok "Deploy dir: $DeployDir ($deployFinal files)"

Write-Step "Creating portable zip"
$ZipPath = "$PackageDir\${AppName}-v${AppVersion}-win64.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path "$DeployDir\*" -DestinationPath $ZipPath -CompressionLevel Optimal
$zipSize = [math]::Round((Get-Item $ZipPath).Length / 1MB, 1)
Write-Ok "Portable: $ZipPath ($zipSize MB)"

Write-Step "Done"
Write-Host ""
Write-Host "Package output: $PackageDir" -ForegroundColor Green
Get-ChildItem $PackageDir -File | ForEach-Object {
    $sizeMB = [math]::Round($_.Length / 1MB, 1)
    Write-Host "  $($_.Name) ($sizeMB MB)"
}
Write-Host "  Deploy dir: $DeployDir ($(Get-ChildItem $DeployDir -Recurse -File | Measure-Object).Count files)" -ForegroundColor Green
