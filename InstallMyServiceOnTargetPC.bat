@set @dummy=0/*
@echo off
NET FILE 1>NUL 2>NUL
if "%ERRORLEVEL%" neq "0" (
  cscript //nologo //E:JScript "%~f0" %*
  exit /b %ERRORLEVEL%
)

REM �Ǘ��Ҍ����Ŏ��s���������� ��������

cd %~dp0

set SERVICENAME="ServiceJikkenSvcName"
set SERVICEEXENAME=ServiceJikken.exe
set SERVICEDISPNAME=%SERVICENAME%
set BINPATH=%~dp0%SERVICEEXENAME%
set DISCRIPTION="Service Jikken Svc Description..."

echo -�T�[�r�X�̃v���Z�X�𖳗����I��������
echo %BINPATH%
taskkill /f /im "%SERVICEEXENAME%"

echo -�T�[�r�X���A���C���X�g�[��
sc delete %SERVICENAME%

echo -�T�[�r�X���C���X�g�[��
rem ����������AbinPath�ɂ̓t���p�X���w�肵�Ȃ��Ƃ��܂������Ȃ�
echo %BINPATH%
sc create "%SERVICENAME%" start=auto binPath= "%BINPATH%" DisplayName= "%SERVICEDISPNAME%"

echo -��������ҏW
sc description %SERVICENAME% %DISCRIPTION%

echo -�T�[�r�X���X�^�[�g
rem sc start %SERVICENAME%
net start %SERVICENAME%
pause

REM �Ǘ��Ҍ����Ŏ��s���������� �����܂�

goto :EOF
*/
var cmd = '"/c ""' + WScript.ScriptFullName + '" ';
for (var i = 0; i < WScript.Arguments.Length; i++) cmd += '"' + WScript.Arguments(i) + '" ';
(new ActiveXObject('Shell.Application')).ShellExecute('cmd.exe', cmd + ' "', '', 'runas', 1);
