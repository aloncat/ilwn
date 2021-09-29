@echo off

echo Syncing S3 bucket 'dmaslov.me' with local copy...
aws s3 sync ..\www s3://dmaslov.me --delete
pause
