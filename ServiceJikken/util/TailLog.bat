@echo off
set TARGET="C:\Log\ServiceJikken.log"
echo %TARGET% ÇÉÇÉjÉ^ÇµÇ‹Ç∑ÅB

rem powershell -NoProfile -ExecutionPolicy unrestricted -Command "Get-Content -Path %TARGET% -Tail 0 -Wait"
powershell -NoProfile -ExecutionPolicy unrestricted -Command "cat %TARGET% -wait"
pause
