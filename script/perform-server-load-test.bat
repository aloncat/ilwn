@echo off

if not exist load-test.tmp md load-test.tmp
if exist load-test.tmp\*.gz del load-test.tmp\*.gz

echo|set /p =This script will produce 3.0 RPS load for 5 minutes & choice
if ERRORLEVEL 2 exit /b

set FP=load-test.tmp\file
set CURL=curl -H "Accept-Encoding: gzip"

for /L %%i in (1 1 60) do (
	echo Loop %%i...
	start /b %CURL% https://dmaslov.me/ -o %FP%-01-%%i.gz -s
	start /b %CURL% https://dmaslov.me/css/default.css -o %FP%-02-%%i.gz -s
	start /b %CURL% https://dmaslov.me/js/pageheader.js -o %FP%-03-%%i.gz -s
	timeout /t 1 /nobreak >nul
	start /b %CURL% https://dmaslov.me/mdpn/ -o %FP%-04-%%i.gz -s
	start /b %CURL% https://dmaslov.me/termsdefs.html -o %FP%-05-%%i.gz -s
	start /b %CURL% https://dmaslov.me/mdpn/checkpal.html -o %FP%-06-%%i.gz -s
	start /b %CURL% https://dmaslov.me/js/checkpal.js -o %FP%-07-%%i.gz -s
	timeout /t 1 /nobreak >nul
	start /b %CURL% https://dmaslov.me/ru/blog/ -o %FP%-08-%%i.gz -s
	start /b %CURL% https://dmaslov.me/mdpn/records.html -o %FP%-09-%%i.gz -s
	start /b %CURL% https://dmaslov.me/js/mdpnrecords.js -o %FP%-10-%%i.gz -s
	timeout /t 1 /nobreak >nul
	start /b %CURL% https://dmaslov.me/mdpn/checkpal.html -o %FP%-11-%%i.gz -s
	start /b %CURL% https://dmaslov.me/about.html -o %FP%-12-%%i.gz -s
	start /b %CURL% https://dmaslov.me/map.html -o %FP%-13-%%i.gz -s
	timeout /t 1 /nobreak >nul
	start /b %CURL% https://dmaslov.me/ru/mdpn/records.html -o %FP%-14-%%i.gz -s
	start /b %CURL% https://dmaslov.me/news.html -o %FP%-15-%%i.gz -s
	timeout /t 1 /nobreak >nul
)

echo Waiting... & timeout /t 5 /nobreak >nul

set FSZ=0
cd load-test.tmp
for /F %%i in ('dir /b *.gz') do set /a FSZ+=%%~zi
echo Done, bytes downloaded: %FSZ% & pause
cd ..
