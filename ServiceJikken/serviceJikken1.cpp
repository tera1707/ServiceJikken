#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include "serviceJikken1.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")

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

// セッション情報取得関数
VOID GetSessionInfo(DWORD sessionId);
// 現在ログイン中のユーザーを確認してログ出力する関数
VOID LogCurrentLoggedInUsers();

// 本プロセスのエントリポイント(メインスレッド)
// ServiceControlのループは、このスレッドで行われる
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    OutputLogToCChokka(L"----------------------------Start------------------------------");
#ifdef _DEBUG
    // デバッグ時にアタッチするための待ち
    //Sleep(10000);
    OutputLogToCChokka(L"-----------------Start(debug wait finished..)------------------");
#endif
    // サービスを登録するためのテーブルを作成(複数のサービスを1プロセスで登録できるようだが今回は1個だけ)
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (LPTSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // サービスがstopしたときにこの関数が制御を返す。
    // このプロセスは、この関数から戻ったら単純に終了すればいい。
    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        OutputLogToCChokka((LPTSTR)(TEXT("StartServiceCtrlDispatcher")));
    }
    return 0;
}

// ServiceControlがSCMから送られてくるたびに呼ばれる
DWORD WINAPI SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    // SvcCtrlMapの安全なアクセス
    OutputLogToCChokka(GetMapValueOrDefault(SvcCtrlMap, dwControl, L"UNKNOWN_SERVICE_CONTROL"));

    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // サービス停止シグナルを送る
        SetEvent(ghSvcStopEvent);
        // 停止を報告
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
    {
        // SessionScMapの安全なアクセス
        OutputLogToCChokka(L" " + GetMapValueOrDefault(SessionScMap, dwEventType, L"UNKNOWN_SESSION_EVENT"));

        // セッション情報を取得
        if (lpEventData != NULL)
        {
            WTSSESSION_NOTIFICATION* sessionNotif = (WTSSESSION_NOTIFICATION*)lpEventData;
            GetSessionInfo(sessionNotif->dwSessionId);
        }

        // 現在ログイン中のユーザーを確認してログ出力
        LogCurrentLoggedInUsers();

        break;
    }

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    case SERVICE_CONTROL_POWEREVENT:
        //OutputLogToCChokka(L" " + std::to_wstring(dwEventType));

        //switch (dwEventType)
        //{
        //case PBT_POWERSETTINGCHANGE:
        //{
        //    auto data = reinterpret_cast<POWERBROADCAST_SETTING*>(lpEventData);

        //    OutputLogToCChokka(L"  " + std::to_wstring(data->PowerSetting.Data1));

        //    if (data->PowerSetting == GUID_BATTERY_PERCENTAGE_REMAINING)
        //    {
        //        auto bat = reinterpret_cast<DWORD*>(data->Data);
        //        auto log = std::wstring(L"   * battery remain is " + std::to_wstring(*bat));
        //        OutputLogToCChokka(log.c_str());
        //    }
        //    else if (data->PowerSetting == GUID_ACDC_POWER_SOURCE)
        //    {
        //        auto src = reinterpret_cast<DWORD*>(data->Data);
        //        std::wstring srcStr = GetMapValueOrDefault(PowSrcMap, *src, L"UNKNOWN_POWER_SOURCE");
        //        auto log = std::wstring(L"   * power source is " + srcStr);
        //        OutputLogToCChokka(log.c_str());
        //    }
        //    break;
        //}
        //}
        break;

    default:
        break;
    }

    return NO_ERROR;
}

// サービスの主たる処理を行うためのスレッド(メインスレッドとは別のスレッド)
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // サービスコントロールを受けれ宇関数を登録(その関数は、メインスレッドで動く)
    gSvcStatusHandle = RegisterServiceCtrlHandlerEx(SVCNAME, SvcCtrlHandler, NULL);

    // 失敗したらサービスが成り立たないので終了させる
    if (!gSvcStatusHandle)
    {
        OutputLogToCChokka((LPTSTR)(TEXT("RegisterServiceCtrlHandler fail...")));
        return;
    }

    // 以下で、デフォルトでは受けないServiceControlを登録する
    // ここではGUID_BATTERY_PERCENTAGE_REMAINING(バッテリの残量変化)を受けられるようにする

    // バッテリーの残量の変化のServiceControlが来るようにする
    auto ret = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_BATTERY_PERCENTAGE_REMAINING, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (!ret)
        return;

    // 電源の変化(AC⇒DC)
    ret = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (!ret)
        return;

    // ----------------------

    // どういうサービスにするか設定
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // SCMにまずは「起動中」であることを報告
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // サービスに必要な各種初期化を行う
    SvcInit(dwArgc, lpszArgv);

    // サービスとしての主たる処理を行う
    SvcMainLoop(dwArgc, lpszArgv);

    SvcEnd(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // ここで、各種変数定義や初期化を行う。
    // サービスのステータスが「SERVICE_START_PENDING」である間は、
    // 定期的にReportSvcStatus()を呼ぶこと。(別に呼ばないまま放置してもエラーになったりはしないが
    // (もし初期化処理でエラーになったりしたときは、そこでSERVICE_STOPPEDを載せてReportSvcStatus()を呼ぶこと)

    // DEBUG用のPending処理()時間のかかる初期化処理を想定
    int ctr = 0;
    for (int i = 0; i < 1; i++)
    {
        //OutputLogToCChokka(L"Pending.." + std::to_wstring(gSvcStatus.dwCheckPoint));
        ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 1000 * 2);
        Sleep(100);
    }

    // サービス終了時に発火させるイベント(SvcMainLoopのループを抜けさせるためのイベント)
    // stopのServiceContorlを受けたらこういったイベント等を使ってループを抜け、処理を終了させる
    ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // 現在ログイン中のセッション情報を取得
    LogCurrentLoggedInUsers();

    // 初期化が終了したので、SCMに「サービスの開始」を報告する
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
}

VOID SvcMainLoop(DWORD dwArgc, LPTSTR* lpszArgv)
{
    while (1)
    {
        // やりたい処理をここでサービス終了までやり続ける
        WaitForSingleObject(ghSvcStopEvent, INFINITE);
        return;
    }
}

VOID SvcEnd(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // 必要な後処理をここで行う

    // サービス終了したら、SERVICE_STOP_PENDINGからSTOPPEDにする
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

// 現在のサービスステータスをセットして、SCMに報告する
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation in milliseconds
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // CsMapの安全なアクセス
    std::wstring stateStr = GetMapValueOrDefault(CsMap, dwCurrentState, L"UNKNOWN_STATE");
    auto msg = stateStr + std::wstring(L": dwCheckPoint = ") + std::to_wstring(dwCheckPoint) + L" dwWaitHint = " + std::to_wstring(dwWaitHint);
    //OutputLogToCChokka(msg);

    // Fill in the SERVICE_STATUS structure.
    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    // Pending中はサービス停止や中断できないようにし、SESSIONCHANGEやPOWER系のSCも受けないようにする
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

// セッション情報を取得する関数
VOID GetSessionInfo(DWORD sessionId)
{
    std::wstring log = L"";
    LPWSTR pBuffer = NULL;
    DWORD bytesReturned = 0;

    // セッションIDをログ出力
    log += L"  Session(" + std::to_wstring(sessionId) + L")";

    // ユーザー名を取得
    if (WTSQuerySessionInformation(
        WTS_CURRENT_SERVER_HANDLE,
        sessionId,
        WTSUserName,
        &pBuffer,
        &bytesReturned))
    {
        if (pBuffer != NULL && wcslen(pBuffer) > 0)
        {
            log += L"  User: " + std::wstring(pBuffer);
        }
        else
        {
            log += L"  User: -";
        }
        WTSFreeMemory(pBuffer);
        pBuffer = NULL;
    }
    else
    {
        log += L"  Failed to get user name. Error: " + std::to_wstring(GetLastError());
    }

    // セッションの状態を取得
    if (WTSQuerySessionInformation(
        WTS_CURRENT_SERVER_HANDLE,
        sessionId,
        WTSConnectState,
        &pBuffer,
        &bytesReturned))
    {
        if (pBuffer != NULL)
        {
            WTS_CONNECTSTATE_CLASS state = *(WTS_CONNECTSTATE_CLASS*)pBuffer;
            std::wstring stateStr;

            switch (state)
            {
            case WTSActive:       stateStr = L"WTSActive"; break;
            case WTSConnected:    stateStr = L"WTSConnected"; break;
            case WTSConnectQuery: stateStr = L"WTSConnectQuery"; break;
            case WTSShadow:       stateStr = L"WTSShadow"; break;
            case WTSDisconnected: stateStr = L"WTSDisconnected"; break;
            case WTSIdle:         stateStr = L"WTSIdle"; break;
            case WTSListen:       stateStr = L"WTSListen"; break;
            case WTSReset:        stateStr = L"WTSReset"; break;
            case WTSDown:         stateStr = L"WTSDown"; break;
            case WTSInit:         stateStr = L"WTSInit"; break;
            default:              stateStr = L"Unknown"; break;
            }

            log += L"  State: " + stateStr;
        }
        WTSFreeMemory(pBuffer);
        pBuffer = NULL;
    }

    OutputLogToCChokka(log);
}

// 現在ログイン中のユーザーを確認してログ出力する関数
VOID LogCurrentLoggedInUsers()
{
    PWTS_SESSION_INFO pSessionInfo = NULL;
    DWORD sessionCount = 0;

    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &sessionCount))
    {
        bool hasLoggedInUser = false;
        std::wstring loggedInUsers;

        for (DWORD i = 0; i < sessionCount; i++)
        {
            if (pSessionInfo[i].State == WTSActive || pSessionInfo[i].State == WTSDisconnected)
            {
                LPWSTR pUserName = NULL;
                DWORD bytesReturned = 0;

                if (WTSQuerySessionInformation(
                    WTS_CURRENT_SERVER_HANDLE,
                    pSessionInfo[i].SessionId,
                    WTSUserName,
                    &pUserName,
                    &bytesReturned))
                {
                    if (pUserName != NULL && wcslen(pUserName) > 0)
                    {
                        if (hasLoggedInUser)
                        {
                            loggedInUsers += L", ";
                        }
                        loggedInUsers += pUserName;
                        loggedInUsers += L"(" + std::to_wstring(pSessionInfo[i].SessionId) + L")";
                        hasLoggedInUser = true;
                    }
                    WTSFreeMemory(pUserName);
                }
            }
        }

        if (hasLoggedInUser)
        {
            OutputLogToCChokka(L"  users: " + loggedInUsers);
        }
        else
        {
            OutputLogToCChokka(L"  No logged in users found");
        }

        WTSFreeMemory(pSessionInfo);
    }
    else
    {
        OutputLogToCChokka(L"  Failed to enumerate sessions. Error: " + std::to_wstring(GetLastError()));
    }
}
