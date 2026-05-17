@echo off

pushd ..
set bp="%cd%\"
popd

if not exist %bp%\bin echo Wrong current directory & exit /b

if [%1]==[] goto :processall
if not exist %1\???-* echo Path "%~1" not found or it has no tasks & exit /b
echo Processing tasks in "%~1" directory...

:processone
for /d %%i in (%1\???-*) do (
  if exist "%%i\state.txt" (
    echo.
    pushd "%%i"
    %bp%bin\guess.exe || (popd & exit /b 1)
    popd
  )
)
exit /b

:processall
echo Path not specified, processing all...

call :processone inwork && ^
call :processone later && ^
call :processone lower
