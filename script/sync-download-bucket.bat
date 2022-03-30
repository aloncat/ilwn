@echo off

echo Syncing bucket 'download.dmaslov.me' with local copy...

set EP=--endpoint-url=https://storage.yandexcloud.net
aws s3 sync ..\..\file\root s3://download.dmaslov.me --delete %EP%
pause
