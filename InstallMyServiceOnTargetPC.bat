@set @dummy=0/*
@echo off
NET FILE 1>NUL 2>NUL
if "%ERRORLEVEL%" neq "0" (
  cscript //nologo //E:JScript "%~f0" %*
  exit /b %ERRORLEVEL%
)

REM 管理者権限で実行したい処理 ここから

cd %~dp0

set SERVICENAME="ServiceJikkenSvcName"
set SERVICEEXENAME=ServiceJikken.exe
set SERVICEDISPNAME=%SERVICENAME%
set BINPATH=%~dp0%SERVICEEXENAME%
set DISCRIPTION="Service Jikken Svc Description..."

echo -サービスのプロセスを無理やり終了させる
echo %BINPATH%
taskkill /f /im "%SERVICEEXENAME%"

echo -サービスをアンインストール
sc delete %SERVICENAME%

echo -サービスをインストール
rem 試した限り、binPathにはフルパスを指定しないとうまくいかない
echo %BINPATH%
sc create "%SERVICENAME%" start=auto binPath= "%BINPATH%" DisplayName= "%SERVICEDISPNAME%"

echo -説明文を編集
sc description %SERVICENAME% %DISCRIPTION%

echo -サービスをスタート
rem sc start %SERVICENAME%
net start %SERVICENAME%
pause

REM 管理者権限で実行したい処理 ここまで

goto :EOF
*/
var cmd = '"/c ""' + WScript.ScriptFullName + '" ';
for (var i = 0; i < WScript.Arguments.Length; i++) cmd += '"' + WScript.Arguments(i) + '" ';
(new ActiveXObject('Shell.Application')).ShellExecute('cmd.exe', cmd + ' "', '', 'runas', 1);
