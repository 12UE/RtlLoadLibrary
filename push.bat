@echo off
chcp 65001 >nul
cd /d "%~dp0"

git add -A
set /p msg="Commit message: "
if "%msg%"=="" set msg=update
git commit -m "%msg%"
git -c http.proxy=http://127.0.0.1:7890 -c https.proxy=http://127.0.0.1:7890 push
pause
