@echo off
setlocal ENABLEDELAYEDEXPANSION
title Deploy Qt 6.x MinGW (Auto-detect EXE)

REM ======================================================
REM CONFIGURACAO
REM ======================================================

set APP_NAME=crono
set DIST_DIR=dist

REM Qt
set QT_VERSION=6.10.1
set QT_ROOT=C:\Qt
set QT_KIT=mingw_64

REM Config files
set SETTINGS_FILE=settings.ini
set QTCONF_FILE=qt.conf

REM ======================================================
REM 1) Localizar automaticamente o EXE Release
REM ======================================================

echo Procurando %APP_NAME%.exe (Release)...

set FOUND_EXE=

for /f "delims=" %%F in ('
    dir /b /s /a:-d "%CD%\build*%APP_NAME%.exe" 2^>nul
') do (
    echo   encontrado: %%F
    set FOUND_EXE=%%F
)

if not defined FOUND_EXE (
    echo ERRO: %APP_NAME%.exe nao encontrado em build*
    goto :END
)

echo Usando:
echo   %FOUND_EXE%
echo.

REM ======================================================
REM 2) Limpar dist
REM ======================================================

echo Limpando pasta %DIST_DIR%...
if exist "%DIST_DIR%" rmdir /S /Q "%DIST_DIR%"
mkdir "%DIST_DIR%"
echo.

REM ======================================================
REM 3) Copiar EXE
REM ======================================================

copy "%FOUND_EXE%" "%DIST_DIR%" > nul
echo Executavel copiado.
echo.

REM ======================================================
REM 4) Copiar arquivos de configuracao
REM ======================================================

if exist "%SETTINGS_FILE%" (
    copy "%SETTINGS_FILE%" "%DIST_DIR%" > nul
    echo settings.ini copiado.
) else (
    echo AVISO: settings.ini nao encontrado.
)

if exist "%QTCONF_FILE%" (
    copy "%QTCONF_FILE%" "%DIST_DIR%" > nul
    echo qt.conf copiado.
) else (
    echo AVISO: qt.conf nao encontrado.
)

echo.

REM ======================================================
REM 5) Rodar windeployqt
REM ======================================================

"%QT_ROOT%\%QT_VERSION%\%QT_KIT%\bin\windeployqt.exe" ^
    --release ^
    "%DIST_DIR%\%APP_NAME%.exe"

echo windeployqt concluido.
echo.

REM ======================================================
REM 6) Copiar dxcompiler / dxil
REM ======================================================

if exist "C:\Windows\System32\dxcompiler.dll" (
    copy "C:\Windows\System32\dxcompiler.dll" "%DIST_DIR%" > nul
    echo dxcompiler.dll copiado.
)

if exist "C:\Windows\System32\dxil.dll" (
    copy "C:\Windows\System32\dxil.dll" "%DIST_DIR%" > nul
    echo dxil.dll copiado.
)

echo.

REM ======================================================
REM 7) Validacao critica
REM ======================================================

if not exist "%DIST_DIR%\platforms\qwindows.dll" (
    echo ERRO: qwindows.dll NAO encontrado!
    goto :END
)

echo qwindows.dll OK.
echo.

REM ======================================================
REM 8) Mostrar arvore final
REM ======================================================

tree "%DIST_DIR%"
echo.

echo ============================================
echo Deploy FINALIZADO com sucesso.
echo Pronto para ZIP e distribuicao.
echo ============================================

:END
pause