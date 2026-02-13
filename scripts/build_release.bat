@echo off
setlocal

pushd "%~dp0\.."
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$proc = Get-Process -Name 'HidTestTool' -ErrorAction SilentlyContinue;" ^
  "if ($proc) { $proc | Stop-Process -Force }"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe';" ^
  "if (!(Test-Path $vswhere)) { Write-Error 'vswhere not found. Please install Visual Studio.'; exit 1 };" ^
  "$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1;" ^
  "if (!$msbuild) { Write-Error 'MSBuild not found.'; exit 1 };" ^
  "& $msbuild 'HidTestTool.sln' /p:Configuration=Release /p:Platform=x64 /m;" ^
  "exit $LASTEXITCODE"
if errorlevel 1 exit /b %errorlevel%
popd

echo Build complete: build\HidTestTool.exe
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$proc = Get-Process -Name 'HidTestTool' -ErrorAction SilentlyContinue;" ^
  "if ($proc) { $proc | Stop-Process -Force; Start-Sleep -Milliseconds 300 };" ^
  "Start-Process -FilePath '%~dp0..\\build\\HidTestTool.exe'"
endlocal
