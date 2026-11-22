@echo off
setlocal
set "ORIGEN=%~dp0ac-songlist"
set "DESTINO=%ProgramData%\obs-studio\plugins\ac-songlist"

echo.
echo  AC Songlist - plugin de pedidos de canciones para OBS Studio
echo.

if not exist "%ORIGEN%\bin\64bit\ac-songlist.dll" (
  echo  No encuentro la carpeta "ac-songlist" junto a este archivo.
  echo  Descomprime el zip completo antes de ejecutar instalar.bat.
  echo.
  pause
  exit /b 1
)

tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | find /I "obs64.exe" >nul
if not errorlevel 1 (
  echo  OBS esta abierto. Cierralo y vuelve a ejecutar este archivo.
  echo.
  pause
  exit /b 1
)

echo  Copiando a "%DESTINO%" ...
xcopy "%ORIGEN%" "%DESTINO%\" /E /I /Y /Q >nul
if errorlevel 1 (
  echo.
  echo  No se pudo copiar. Prueba con clic derecho ^> Ejecutar como administrador.
  echo.
  pause
  exit /b 1
)

echo.
echo  Listo. Abre OBS y activa el panel en el menu Paneles ^> AC Songlist.
echo  Tus canciones se guardan aparte: reinstalar o actualizar no las borra.
echo.
pause
