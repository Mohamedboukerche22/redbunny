# RedBunny one-line installer for Windows.
#
#   irm https://raw.githubusercontent.com/Mohamedboukerche22/redbunny/main/scripts/get.ps1 | iex
#
# Downloads the repository, installs the compiler, then compiles and runs
# examples\hello.redb. Installs under %LOCALAPPDATA%\RedBunny by default;
# override with $env:PREFIX.

$ErrorActionPreference = "Stop"

$Repo = if ($env:REDB_REPO) { $env:REDB_REPO } else { "Mohamedboukerche22/redbunny" }
$Branch = if ($env:REDB_BRANCH) { $env:REDB_BRANCH } else { "main" }
$Prefix = if ($env:PREFIX) { $env:PREFIX } else { Join-Path $env:LOCALAPPDATA "RedBunny" }
$ArchiveUrl = if ($env:REDB_ARCHIVE_URL) {
    $env:REDB_ARCHIVE_URL
} else {
    "https://github.com/$Repo/archive/refs/heads/$Branch.zip"
}

$Tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("redbunny-" + [guid]::NewGuid().ToString("N"))
$OriginalLocation = Get-Location
$PreviousPrefix = $env:PREFIX
New-Item -ItemType Directory -Path $Tmp -Force | Out-Null

try {
    [Net.ServicePointManager]::SecurityProtocol = `
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

    $Zip = Join-Path $Tmp "redbunny.zip"
    Write-Host "Downloading RedBunny ($Branch)..."
    Invoke-WebRequest -Uri $ArchiveUrl -OutFile $Zip -UseBasicParsing

    $Extract = Join-Path $Tmp "src"
    Expand-Archive -Path $Zip -DestinationPath $Extract -Force

    $SourceDir = Get-ChildItem -Path $Extract -Directory | Select-Object -First 1
    if (-not $SourceDir) {
        throw "downloaded archive is empty"
    }

    $env:PREFIX = $Prefix
    Write-Host "Installing to $Prefix..."
    & (Join-Path $SourceDir.FullName "scripts\install.ps1")

    $Redb = Join-Path $Prefix "bin\redb.exe"
    if (-not (Test-Path $Redb)) {
        throw "compiler not found at $Redb"
    }

    $Hello = Join-Path $Tmp "hello.exe"
    Write-Host ""
    Write-Host "Compiling and running examples\hello.redb..."
    & $Redb (Join-Path $SourceDir.FullName "examples\hello.redb") -o $Hello
    if ($LASTEXITCODE -ne 0) { throw "example compilation failed" }

    & $Hello
    if ($LASTEXITCODE -ne 0) { throw "example failed to run" }

    Write-Host ""
    Write-Host "Installed: $Redb" -ForegroundColor Green
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath -notlike "*$Prefix\bin*") {
        Write-Host "Add it to PATH for this session:"
        Write-Host "  `$env:Path = `"$Prefix\bin;`$env:Path`""
    }
}
finally {
    Set-Location $OriginalLocation
    if ($null -eq $PreviousPrefix) {
        Remove-Item Env:PREFIX -ErrorAction SilentlyContinue
    } else {
        $env:PREFIX = $PreviousPrefix
    }
    Remove-Item -Path $Tmp -Recurse -Force -ErrorAction SilentlyContinue
}
