
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

rem サービスのプロセスを無理やり終了させる
taskkill /f /im ServiceJikken.exe
timeout /t 1 /nobreak > nul

rem サービスをアンインストール
sc delete %SERVICENAME%
timeout /t 1 /nobreak > nul

rem サービスをインストール
rem 試した限り、binPathにはフルパスを指定しないとうまくいかない
sc create %SERVICENAME% start=auto binPath= "%BINPATH%" DisplayName= %SERVICEDISPNAME%
timeout /t 1 /nobreak > nul

rem 説明文を編集
sc description %SERVICENAME% %DISCRIPTION%
timeout /t 1 /nobreak > nul

mkdir "C:\Log" 2>nul

rem サービスをスタート
rem sc start %SERVICENAME%
net start %SERVICENAME%
timeout /t 1 /nobreak > nul

rem サービスをストップ
rem sc stop %SERVICENAME%
rem timeout /t 1 /nobreak > nul

pause

REM 管理者権限で実行したい処理 ここまで

goto :EOF
*/
var cmd = '"/c ""' + WScript.ScriptFullName + '" ';
for (var i = 0; i < WScript.Arguments.Length; i++) cmd += '"' + WScript.Arguments(i) + '" ';
(new ActiveXObject('Shell.Application')).ShellExecute('cmd.exe', cmd + ' "', '', 'runas', 1);
