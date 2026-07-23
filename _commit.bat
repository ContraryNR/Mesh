@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo [Mesh Project] Git Commit Script
echo =================================

echo Setting up Git proxy...
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890
echo Proxy configured: http://127.0.0.1:7890

echo.
echo Git status:
git status

echo.
echo [0] Cancel
echo [1] Use timestamp as commit message
echo [2] Input commit message manually
set /p choice="Choose (0/1/2): "

if "%choice%"=="0" (
    echo Cancelled.
    pause
    exit /b 0
) else if "%choice%"=="1" (
    for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
    set timestamp=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2% %datetime:~8,2%:%datetime:~10,2%:%datetime:~12,2%
    set commitMsg=Auto commit: %timestamp%
) else if "%choice%"=="2" (
    set /p commitMsg="Input commit message: "
    if "!commitMsg!"=="" (
        echo Error: commit message cannot be empty!
        pause
        exit /b 1
    )
) else (
    echo Invalid choice!
    pause
    exit /b 1
)

echo.
echo Committing...
git add .
git commit -m "%commitMsg%"

if %errorlevel% equ 0 (
    echo.
    echo Commit succeeded!
) else (
    echo.
    echo Commit failed!
)

pause