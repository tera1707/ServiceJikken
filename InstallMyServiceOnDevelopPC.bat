@set @dummy=0/*
@echo off
NET FILE 1>NUL 2>NUL
if "%ERRORLEVEL%" neq "0" (
  cscript //nologo //E:JScript "%~f0" %*
  exit /b %ERRORLEVEL%
)

REM 管理者権限で実行したい処理 ここから

cd %~dp0

set SOLUTIONPATH="%~dp0ServiceJikken.sln"

set SERVICENAME="ServiceJikkenSvcName"
set SERVICEDISPNAME=%SERVICENAME%
set BINPATH=%~dp0x64\Debug\ServiceJikken.exe
set DISCRIPTION="Service Jikken Svc Description..."

call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

echo -サービスのプロセスを無理やり終了させる
echo %BINPATH%
taskkill /f /im %BINPATH%

echo -サービスをアンインストール
sc delete %SERVICENAME%

echo -ソリューションをリビルドする
MSBuild %SOLUTIONPATH% /t:clean;rebuild /p:Configuration=Debug;Platform="x64" > nul
if %ERRORLEVEL% neq 0 (
    echo ErrorLevel:%ERRORLEVEL%
    echo ビルド失敗
    pause
    exit
)
echo -ビルド成功

echo -サービスをインストール
rem 試した限り、binPathにはフルパスを指定しないとうまくいかない
sc create %SERVICENAME% start=auto binPath= %BINPATH% DisplayName= %SERVICEDISPNAME%

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
