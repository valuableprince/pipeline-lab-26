@echo off
setlocal
cd /d "%~dp0\.."
if "%TARKENSO_GAME_SERVER%"=="" (
  echo ERROR: set TARKENSO_GAME_SERVER=host:port
  exit /b 2
)
if "%TARKENSO_DB_HOST%"=="" (
  echo ERROR: set TARKENSO_DB_HOST
  exit /b 2
)
if "%TARKENSO_DB_USER%"=="" (
  echo ERROR: set TARKENSO_DB_USER
  exit /b 2
)
if "%TARKENSO_DB_NAME%"=="" (
  echo ERROR: set TARKENSO_DB_NAME
  exit /b 2
)
if "%TARKENSO_LOAD_PASSWORD%"=="" (
  echo ERROR: set TARKENSO_LOAD_PASSWORD for test-only accounts
  exit /b 2
)
if not exist "rakclient.exe" (
  echo ERROR: place a compatible rakclient.exe in the repository directory
  exit /b 2
)
echo TARKENSO LoadNet Worker - press Ctrl+C to stop
python tools\loadnet_worker.py --server "%TARKENSO_GAME_SERVER%" --confirm-target "%TARKENSO_GAME_SERVER%" --client-bin "rakclient.exe"
exit /b %errorlevel%
