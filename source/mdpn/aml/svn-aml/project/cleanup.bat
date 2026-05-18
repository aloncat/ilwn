@echo off

call :clean core
exit /b

:clean
set PRJ="%~dp0%1"
if [%1]==[] exit /b
if not exist %PRJ% echo Path %PRJ% not found & exit /b
if exist %PRJ%\debug rd /s/q %PRJ%\debug
if exist %PRJ%\release rd /s/q %PRJ%\release
if exist %PRJ%\*.user del %PRJ%\*.user
exit /b
