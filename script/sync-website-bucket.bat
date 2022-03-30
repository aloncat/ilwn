@echo off

echo Syncing bucket 'www.dmaslov.me' with local copy...

set EP=--endpoint-url=https://storage.yandexcloud.net
aws s3 sync ..\www s3://www.dmaslov.me --delete --exclude "*.js" %EP%
aws s3 sync ..\www s3://www.dmaslov.me --delete --exclude "*" --include "*.js" --content-type "text/javascript" %EP%
pause
