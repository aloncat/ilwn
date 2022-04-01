@echo off

echo Syncing bucket 'dl.dmaslov.me' with local copy...

set EP=--endpoint-url=https://storage.yandexcloud.net
aws s3 sync ..\..\file\root s3://dl.dmaslov.me --delete %EP%
pause
