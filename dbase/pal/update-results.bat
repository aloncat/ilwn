@echo off

if exist results.bak (
  if exist results.txt (
    fc /b results.txt results.bak >nul
    if errorlevel 1 (
      echo Please, remove results.bak and try again
      goto :end
    )
    del results.bak
  )
)

if not exist results.txt (
  type nul >results.txt
) else (
  copy results.txt results.bak >nul || goto :end
)

cd ..
set bp="%cd%\"

rem call :list 20
call :list 21
call :list 22
call :list 23
call :list 24

cd pal
if exist results.bak (
  echo.
  fc /b results.txt results.bak >nul
  if errorlevel 1 (
    echo Update complete. There are some changes!
  ) else (
    echo Update complete. No changes made
    del results.bak
  )
)

:end
pause
exit /b

:list
pushd %1 || (
  echo WARNING! Directory %1 not found
  exit /b
)
echo.
echo Processing folder %1...
move %bp%pal\results.txt . >nul && (
  %bp%bin\list.exe
  move results.txt %bp%pal >nul
)
popd
exit /b
