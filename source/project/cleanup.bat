@echo off

if exist "%~dp0.vs" rd /s/q "%~dp0.vs"

call :clean eslp
call :clean lab
call ..\aml\project\cleanup.bat
exit /b

:clean
set PRJ="%~dp0%1"
if [%1]==[] exit /b
if not exist %PRJ% echo Path %PRJ% not found & exit /b
if exist %PRJ%\debug rd /s/q %PRJ%\debug
if exist %PRJ%\release rd /s/q %PRJ%\release
if exist %PRJ%\*.user del %PRJ%\*.user
exit /b
