
@set @dummy=0/*
@echo off
NET FILE 1>NUL 2>NUL
if "%ERRORLEVEL%" neq "0" (
  cscript //nologo //E:JScript "%~f0" %*
  exit /b %ERRORLEVEL%
)

REM 管理者権限で実行したい処理 ここから

set SERVICENAME="ServiceJikkenSvcName"
set SERVICEDISPNAME=%SERVICENAME%
set BINPATH=%~dp0\ServiceJikken.exe
set DISCRIPTION="Service Jikken"

rem サービスをスタート
rem sc start %SERVICENAME%
rem net start %SERVICENAME%
rem timeout /t 1 /nobreak > nul

rem サービスをストップ
sc stop %SERVICENAME%
timeout /t 3 /nobreak > nul

REM 管理者権限で実行したい処理 ここまで

goto :EOF
*/
var cmd = '"/c ""' + WScript.ScriptFullName + '" ';
for (var i = 0; i < WScript.Arguments.Length; i++) cmd += '"' + WScript.Arguments(i) + '" ';
(new ActiveXObject('Shell.Application')).ShellExecute('cmd.exe', cmd + ' "', '', 'runas', 1);
