@echo off

if exist all-palindromes.bak (
  if exist all-palindromes.txt (
    fc /b all-palindromes.txt all-palindromes.bak >nul
    if errorlevel 1 (
      echo Please, remove 'all-palindromes.bak' and try again
      goto end
    )
    del all-palindromes.bak
  )
)

if not exist all-palindromes.txt (
  type nul >all-palindromes.txt
) else (
  copy all-palindromes.txt all-palindromes.bak >nul || goto end
)

cd ..
set bp="%cd%\"

rem call :list 18
rem call :list 19
rem call :list 20
call :list 21
call :list 22
call :list 23
call :list 24

cd pal
if exist all-palindromes.bak (
  echo.
  fc /b all-palindromes.txt all-palindromes.bak >nul
  if errorlevel 1 (
    echo Update complete. There are some changes!
  ) else (
    echo Update complete. No changes made
    del all-palindromes.bak
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
move %bp%pal\all-palindromes.txt . >nul && (
  %bp%bin\list.exe
  move all-palindromes.txt %bp%pal >nul
)
popd
exit /b
