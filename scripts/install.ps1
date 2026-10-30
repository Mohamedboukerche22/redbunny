# RedBunny compiler installer for Windows.
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Need($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

if (-not (Need choco)) {
    Write-Host "Installing Chocolatey..."
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = `
        [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString(
        'https://community.chocolatey.org/install.ps1'))
}

if (-not (Need cmake) -or -not (Need clang) -or -not (Need ninja)) {
    Write-Host "Installing LLVM, CMake and Ninja..."
    choco install llvm cmake ninja -y
}

# Chocolatey installs LLVM here by default.
$llvmRoot = "C:\Program Files\LLVM"
if (Test-Path "$llvmRoot\bin") {
    $env:Path = "$llvmRoot\bin;$env:Path"
}

$cmakeArgs = @(
    "-S", $Root,
    "-B", (Join-Path $Root "build"),
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release"
)

if (Test-Path "$llvmRoot\lib\cmake\llvm\LLVMConfig.cmake") {
    $cmakeArgs += "-DLLVM_DIR=$llvmRoot\lib\cmake\llvm"
}

Write-Host "Configuring RedBunny..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "Building RedBunny..."
& cmake --build (Join-Path $Root "build")
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$prefix = "C:\Program Files\RedBunny"
Write-Host "Installing to $prefix..."
& cmake --install (Join-Path $Root "build") --prefix $prefix
if ($LASTEXITCODE -ne 0) { throw "install failed" }

$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if ($machinePath -notlike "*$prefix\bin*") {
    [Environment]::SetEnvironmentVariable(
        "Path", "$machinePath;$prefix\bin", "Machine")
}

Write-Host ""
Write-Host "RedBunny installed." -ForegroundColor Green
Write-Host "Open a new terminal, then run:"
Write-Host "  redb --version"
Write-Host "  redb examples\hello.redb -o hello.exe"
Write-Host "  .\hello.exe"
