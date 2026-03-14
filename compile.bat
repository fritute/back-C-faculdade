@echo off
set GCC_PATH=gcc
set LIBS=-lws2_32

echo.
echo ==========================================
echo Compiling DistribPro Professional Backend
echo ==========================================
echo.

:: Directories
set SRC_DIR=src
set INC_DIR=include
set LIB_DIR=libs
set BUILD_DIR=build
set BIN_DIR=bin

:: Create build/bin directories if they don't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%

:: Source Files
set SOURCES=%SRC_DIR%\main.c ^
    %SRC_DIR%\db.c ^
    %SRC_DIR%\http\server.c ^
    %SRC_DIR%\http\handlers\produtos.c ^
    %SRC_DIR%\http\handlers\clientes.c ^
    %SRC_DIR%\http\handlers\fornecedores.c ^
    %SRC_DIR%\http\handlers\pedidos.c ^
    %SRC_DIR%\http\handlers\dashboard.c ^
    %SRC_DIR%\http\handlers\estoque.c ^
    %SRC_DIR%\http\handlers\auth.c ^
    %SRC_DIR%\http\handlers\config.c ^
    %SRC_DIR%\db\repo.c ^
    %SRC_DIR%\utils\common.c ^
    %LIB_DIR%\mongoose.c ^
    %LIB_DIR%\cJSON.c

:: PostgreSQL Paths (MSYS2 ucrt64 - onde o GCC esta instalado)
set PG_PATH=C:\msys64\ucrt64
set PG_INCLUDE="%PG_PATH%\include"
set PG_LIB="%PG_PATH%\lib"

:: Include Paths
set INCLUDES=-I%INC_DIR% -I%LIB_DIR% -I%PG_INCLUDE%

echo [1/2] Compiling sources...
%GCC_PATH% %SOURCES% %INCLUDES% %LIBS% -L%PG_LIB% -lpq -o %BIN_DIR%\distribpro.exe

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Compilation failed!
    exit /b %errorlevel%
)

echo [2/2] Done!
echo.
echo ==========================================
echo Binary created at: %BIN_DIR%\distribpro.exe
echo ==========================================
echo.
pause
