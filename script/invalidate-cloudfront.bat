@echo off

echo Creating invalidation for 'dmaslov.me/*'...
aws cloudfront create-invalidation ^
	--distribution-id E2J733JN1ZXP5M ^
	--paths "/*"
pause
