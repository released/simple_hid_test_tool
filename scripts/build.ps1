param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [ValidateSet("Win32", "x64")][string]$Platform = "x64"
)

$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\verify_repo_identity.ps1" -SourceOnly -InstallHook

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Please run from a Visual Studio Developer PowerShell or install Visual Studio."
    exit 1
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    Write-Error "MSBuild not found."
    exit 1
}

& $msbuild "HidTestTool.sln" /p:Configuration=$Configuration /p:Platform=$Platform /m
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Join-Path "build" "HidTestTool.exe"
if (Test-Path $exe) {
    Write-Host "Built: $exe"
} else {
    Write-Warning "Build succeeded but $exe not found."
}
