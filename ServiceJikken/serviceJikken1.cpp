#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include "serviceJikken1.h"

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("SvcName")

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

DWORD WINAPI SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcMainLoop(DWORD, LPTSTR*);
VOID SvcEnd(DWORD, LPTSTR*);

// �{�v���Z�X�̃G���g���|�C���g(���C���X���b�h)
// ServiceControl�̃��[�v�́A���̃X���b�h�ōs����
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    OutputLogToCChokka(L"----------------------------Start------------------------------");
#ifdef _DEBUG
    // �f�o�b�O���ɃA�^�b�`���邽�߂̑҂�
    //Sleep(10000);
    OutputLogToCChokka(L"-----------------Start(debug wait finished..)------------------");
#endif
    // �T�[�r�X��o�^���邽�߂̃e�[�u�����쐬(�����̃T�[�r�X��1�v���Z�X�œo�^�ł���悤���������1����)
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (LPTSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // �T�[�r�X��stop�����Ƃ��ɂ��̊֐��������Ԃ��B
    // ���̃v���Z�X�́A���̊֐�����߂�����P���ɏI������΂����B
    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        OutputLogToCChokka((LPTSTR)(TEXT("StartServiceCtrlDispatcher")));
    }
    return 0;
}

// ServiceControl��SCM���瑗���Ă��邽�тɌĂ΂��
DWORD WINAPI SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    // SvcCtrlMap�̈��S�ȃA�N�Z�X
    OutputLogToCChokka(GetMapValueOrDefault(SvcCtrlMap, dwControl, L"UNKNOWN_SERVICE_CONTROL"));

    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // �T�[�r�X��~�V�O�i���𑗂�
        SetEvent(ghSvcStopEvent);
        // ��~���
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
    {
        // SessionScMap�̈��S�ȃA�N�Z�X
        OutputLogToCChokka(L" " + GetMapValueOrDefault(SessionScMap, dwEventType, L"UNKNOWN_SESSION_EVENT"));
        break;
    }

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    case SERVICE_CONTROL_POWEREVENT:
        OutputLogToCChokka(L" " + std::to_wstring(dwEventType));

        switch (dwEventType)
        {
        case PBT_POWERSETTINGCHANGE:
        {
            auto data = reinterpret_cast<POWERBROADCAST_SETTING*>(lpEventData);

            OutputLogToCChokka(L"  " + std::to_wstring(data->PowerSetting.Data1));

            if (data->PowerSetting == GUID_BATTERY_PERCENTAGE_REMAINING)
            {
                auto bat = reinterpret_cast<DWORD*>(data->Data);
                auto log = std::wstring(L"   * battery remain is " + std::to_wstring(*bat));
                OutputLogToCChokka(log.c_str());
            }
            else if (data->PowerSetting == GUID_ACDC_POWER_SOURCE)
            {
                auto src = reinterpret_cast<DWORD*>(data->Data);
                std::wstring srcStr = GetMapValueOrDefault(PowSrcMap, *src, L"UNKNOWN_POWER_SOURCE");
                auto log = std::wstring(L"   * power source is " + srcStr);
                OutputLogToCChokka(log.c_str());
            }
            break;
        }
        }
        break;

    default:
        break;
    }

    return NO_ERROR;
}

// �T�[�r�X�̎傽�鏈�����s�����߂̃X���b�h(���C���X���b�h�Ƃ͕ʂ̃X���b�h)
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // �T�[�r�X�R���g���[�����󂯂�F�֐���o�^(���̊֐��́A���C���X���b�h�œ���)
    gSvcStatusHandle = RegisterServiceCtrlHandlerEx(SVCNAME, SvcCtrlHandler, NULL);

    // ���s������T�[�r�X�����藧���Ȃ��̂ŏI��������
    if (!gSvcStatusHandle)
    {
        OutputLogToCChokka((LPTSTR)(TEXT("RegisterServiceCtrlHandler fail...")));
        return;
    }

    // �ȉ��ŁA�f�t�H���g�ł͎󂯂Ȃ�ServiceControl��o�^����
    // �����ł�GUID_BATTERY_PERCENTAGE_REMAINING(�o�b�e���̎c�ʕω�)���󂯂���悤�ɂ���

    // �o�b�e���[�̎c�ʂ̕ω���ServiceControl������悤�ɂ���
    auto ret = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_BATTERY_PERCENTAGE_REMAINING, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (!ret)
        return;

    // �d���̕ω�(AC��DC)
    ret = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (!ret)
        return;

    // ----------------------

    // �ǂ������T�[�r�X�ɂ��邩�ݒ�
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // SCM�ɂ܂��́u�N�����v�ł��邱�Ƃ��
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // �T�[�r�X�ɕK�v�Ȋe�평�������s��
    SvcInit(dwArgc, lpszArgv);

    // �T�[�r�X�Ƃ��Ă̎傽�鏈�����s��
    SvcMainLoop(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // �����ŁA�e��ϐ���`�⏉�������s���B
    // �T�[�r�X�̃X�e�[�^�X���uSERVICE_START_PENDING�v�ł���Ԃ́A
    // ����I��ReportSvcStatus()���ĂԂ��ƁB(�ʂɌĂ΂Ȃ��܂ܕ��u���Ă��G���[�ɂȂ�����͂��Ȃ���
    // (���������������ŃG���[�ɂȂ����肵���Ƃ��́A������SERVICE_STOPPED���ڂ���ReportSvcStatus()���ĂԂ���)

    // DEBUG�p��Pending����()���Ԃ̂����鏉����������z��
    int ctr = 0;
    for (int i = 0; i < 5; i++)
    {
        OutputLogToCChokka(L"Pending.." + std::to_wstring(gSvcStatus.dwCheckPoint));
        ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 1000 * 2);
        Sleep(1000);
    }

    // �T�[�r�X�I�����ɔ��΂�����C�x���g(SvcMainLoop�̃��[�v�𔲂������邽�߂̃C�x���g)
    // stop��ServiceContorl���󂯂��炱���������C�x���g�����g���ă��[�v�𔲂��A�������I��������
    ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // ���������I�������̂ŁASCM�Ɂu�T�[�r�X�̊J�n�v��񍐂���
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
}

VOID SvcMainLoop(DWORD dwArgc, LPTSTR* lpszArgv)
{
    while (1)
    {
        // ��肽�������������ŃT�[�r�X�I���܂ł�葱����
        WaitForSingleObject(ghSvcStopEvent, INFINITE);
        return;
    }
}

VOID SvcEnd(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // �K�v�Ȍ㏈���������ōs��

    // �T�[�r�X�I��������ASERVICE_STOP_PENDING����STOPPED�ɂ���
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

// ���݂̃T�[�r�X�X�e�[�^�X���Z�b�g���āASCM�ɕ񍐂���
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation in milliseconds
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // CsMap�̈��S�ȃA�N�Z�X
    std::wstring stateStr = GetMapValueOrDefault(CsMap, dwCurrentState, L"UNKNOWN_STATE");
    auto msg = stateStr + std::wstring(L": dwCheckPoint = ") + std::to_wstring(dwCheckPoint) + L" dwWaitHint = " + std::to_wstring(dwWaitHint);
    OutputLogToCChokka(msg);

    // Fill in the SERVICE_STATUS structure.
    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    // Pending���̓T�[�r�X��~�⒆�f�ł��Ȃ��悤�ɂ��ASESSIONCHANGE��POWER�n��SC���󂯂Ȃ��悤�ɂ���
    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_POWEREVENT;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}
