@echo off

echo Syncing S3 bucket 'dmaslov.me' with local copy...
aws s3 sync ..\www s3://dmaslov.me --delete --exclude "*.js"
aws s3 sync ..\www s3://dmaslov.me --delete --exclude "*" --include "*.js" --content-type "text/javascript"
pause
