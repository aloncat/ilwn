@echo off

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 goto elevate

reg add HKLM\Software\Classes\.js /v "Content Type" /t REG_SZ /d "text/javascript" /f
pause
exit /b

:elevate
cd /d "%~dp0"
powershell -Command "Start-Process '%~nx0' -Verb RunAs"
