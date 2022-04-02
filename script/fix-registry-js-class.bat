@echo off

net session
if %ERRORLEVEL% NEQ 0 goto elevate

cls
reg add HKLM\Software\Classes\.js /v "Content Type" /t REG_SZ /d "text/javascript" /f
pause & exit /b

:elevate
cd /d "%~dp0"
mshta "javascript: var shell = new ActiveXObject('shell.application'); shell.ShellExecute('%~nx0', '', '', 'runas', 1); close();"
