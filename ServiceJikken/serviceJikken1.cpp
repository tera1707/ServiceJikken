#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include <shlwapi.h>
#include "serviceJikken1.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

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
// 画面表示を90度回転させる関数
VOID RotateDisplay90Degrees();
// ユーザーセッションでプロセスを起動する関数
BOOL LaunchProcessInUserSession(DWORD sessionId, LPWSTR commandLine);
// ログオン画面でプロセスを起動する関数
BOOL LaunchProcessOnLogonScreen();

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

        // 画面表示を90度回転（ユーザーセッションでDisplayRotator.exeを起動）
        RotateDisplay90Degrees();

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



    // 現在ログイン中のユーザーを確認してログ出力
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
    LPWSTR pBuffer = NULL;
    DWORD bytesReturned = 0;

    // セッションIDをログ出力
    OutputLogToCChokka(L"  Session ID: " + std::to_wstring(sessionId));

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
            OutputLogToCChokka(L"  User Name: " + std::wstring(pBuffer));
        }
        else
        {
            OutputLogToCChokka(L"  User Name: (none)");
        }
        WTSFreeMemory(pBuffer);
        pBuffer = NULL;
    }
    else
    {
        OutputLogToCChokka(L"  Failed to get user name. Error: " + std::to_wstring(GetLastError()));
    }

    // ドメイン名を取得
    if (WTSQuerySessionInformation(
        WTS_CURRENT_SERVER_HANDLE,
        sessionId,
        WTSDomainName,
        &pBuffer,
        &bytesReturned))
    {
        if (pBuffer != NULL && wcslen(pBuffer) > 0)
        {
            OutputLogToCChokka(L"  Domain Name: " + std::wstring(pBuffer));
        }
        WTSFreeMemory(pBuffer);
        pBuffer = NULL;
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

            OutputLogToCChokka(L"  Session State: " + stateStr);
        }
        WTSFreeMemory(pBuffer);
        pBuffer = NULL;
    }
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
                        loggedInUsers += L" (SessionID: " + std::to_wstring(pSessionInfo[i].SessionId) + L")";
                        hasLoggedInUser = true;
                    }
                    WTSFreeMemory(pUserName);
                }
            }
        }

        if (hasLoggedInUser)
        {
            OutputLogToCChokka(L"  Logged in users: " + loggedInUsers);
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

// 画面を1回90度回転させる内部関数
BOOL RotateDisplayOnce()
{
    DEVMODE devMode;
    ZeroMemory(&devMode, sizeof(devMode));
    devMode.dmSize = sizeof(devMode);

    // プライマリディスプレイの現在の設定を取得
    if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode))
    {
        OutputLogToCChokka(L"  Failed to get current display settings. Error: " + std::to_wstring(GetLastError()));
        return FALSE;
    }

    // 現在の向きを取得
    DWORD currentOrientation = devMode.dmDisplayOrientation;

    // 90度回転させる（現在の向きから+90度）
    DWORD newOrientation;
    switch (currentOrientation)
    {
    case DMDO_DEFAULT:  // 0度
        newOrientation = DMDO_90;
        break;
    case DMDO_90:       // 90度
        newOrientation = DMDO_180;
        break;
    case DMDO_180:      // 180度
        newOrientation = DMDO_270;
        break;
    case DMDO_270:      // 270度
        newOrientation = DMDO_DEFAULT;
        break;
    default:
        newOrientation = DMDO_90;
        break;
    }

    // 新しい向きを設定（幅と高さは指定しない - システムが自動で調整）
    devMode.dmDisplayOrientation = newOrientation;
    devMode.dmFields = DM_DISPLAYORIENTATION;

    OutputLogToCChokka(L"  Rotating display from " + std::to_wstring(currentOrientation) +
        L" to " + std::to_wstring(newOrientation));

    // ディスプレイ設定を変更
    // CDS_RESET: 即座に変更を適用（レジストリには保存しない）
    LONG result = ChangeDisplaySettingsEx(NULL, &devMode, NULL, CDS_RESET, NULL);

    switch (result)
    {
    case DISP_CHANGE_SUCCESSFUL:
        OutputLogToCChokka(L"  Display rotation successful");
        return TRUE;
    case DISP_CHANGE_RESTART:
        OutputLogToCChokka(L"  Display rotation requires restart");
        return FALSE;
    case DISP_CHANGE_FAILED:
        OutputLogToCChokka(L"  Display rotation failed");
        return FALSE;
    case DISP_CHANGE_BADMODE:
        OutputLogToCChokka(L"  Display rotation failed: Bad mode");
        return FALSE;
    case DISP_CHANGE_NOTUPDATED:
        OutputLogToCChokka(L"  Display rotation failed: Not updated");
        return FALSE;
    case DISP_CHANGE_BADFLAGS:
        OutputLogToCChokka(L"  Display rotation failed: Bad flags");
        return FALSE;
    case DISP_CHANGE_BADPARAM:
        OutputLogToCChokka(L"  Display rotation failed: Bad parameter");
        return FALSE;
    default:
        OutputLogToCChokka(L"  Display rotation unknown result: " + std::to_wstring(result));
        return FALSE;
    }
}

// 5秒おきに4回、90度回転させる関数（ユーザーセッションでDisplayRotator.exeを起動）
VOID RotateDisplay90Degrees()
{
    OutputLogToCChokka(L"  Starting display rotation by launching DisplayRotator.exe in user session");

    // アクティブなユーザーセッションを見つける
    PWTS_SESSION_INFO pSessionInfo = NULL;
    DWORD sessionCount = 0;
    DWORD targetSessionId = 0xFFFFFFFF; // 無効な値で初期化
    BOOL foundActiveSession = FALSE;

    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &sessionCount))
    {
        for (DWORD i = 0; i < sessionCount; i++)
        {
            if (pSessionInfo[i].State == WTSActive)
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
                        targetSessionId = pSessionInfo[i].SessionId;
                        foundActiveSession = TRUE;
                        OutputLogToCChokka(L"  Found active user session: SessionID=" + std::to_wstring(targetSessionId) + L", User=" + std::wstring(pUserName));
                    }
                    WTSFreeMemory(pUserName);
                }

                if (foundActiveSession)
                    break;
            }
        }

        WTSFreeMemory(pSessionInfo);
    }

    if (!foundActiveSession)
    {
        OutputLogToCChokka(L"  No active user session found. Trying to launch on Logon screen instead.");

        // ログオン画面でDisplayRotatorを起動
        if (LaunchProcessOnLogonScreen())
        {
            OutputLogToCChokka(L"  DisplayRotator.exe launched on Logon screen successfully");
        }
        else
        {
            OutputLogToCChokka(L"  Failed to launch DisplayRotator.exe on Logon screen");
        }

        return;
    }

    // DisplayRotator.exeのパス（サービスと同じディレクトリにあると仮定）
    WCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath);
    wcscat_s(exePath, L"\\DisplayRotator.exe");

    // コマンドライン（引数として回転回数を渡す）
    WCHAR commandLine[MAX_PATH];
    wsprintf(commandLine, L"\"%s\" 4", exePath);

    OutputLogToCChokka(L"  Launching: " + std::wstring(commandLine));

    // ユーザーセッションでプロセスを起動
    if (LaunchProcessInUserSession(targetSessionId, commandLine))
    {
        OutputLogToCChokka(L"  DisplayRotator.exe launched successfully");
    }
    else
    {
        OutputLogToCChokka(L"  Failed to launch DisplayRotator.exe");
    }
}

// ユーザーセッションでプロセスを起動する関数
BOOL LaunchProcessInUserSession(DWORD sessionId, LPWSTR commandLine)
{
    HANDLE hUserToken = NULL;
    HANDLE hDuplicatedToken = NULL;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL result = FALSE;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.lpDesktop = (LPWSTR)L"winsta0\\default"; // ユーザーのデスクトップを指定

    ZeroMemory(&pi, sizeof(pi));

    // セッションのユーザートークンを取得
    if (!WTSQueryUserToken(sessionId, &hUserToken))
    {
        OutputLogToCChokka(L"  WTSQueryUserToken failed. Error: " + std::to_wstring(GetLastError()));
        return FALSE;
    }

    // トークンを複製（プライマリトークンとして）
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(
        hUserToken,
        MAXIMUM_ALLOWED,
        &sa,
        SecurityIdentification,
        TokenPrimary,
        &hDuplicatedToken))
    {
        OutputLogToCChokka(L"  DuplicateTokenEx failed. Error: " + std::to_wstring(GetLastError()));
        CloseHandle(hUserToken);
        return FALSE;
    }

    // プロセスを起動
    if (CreateProcessAsUser(
        hDuplicatedToken,
        NULL,
        commandLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS,
        NULL,
        NULL,
        &si,
        &pi))
    {
        OutputLogToCChokka(L"  CreateProcessAsUser succeeded. ProcessID: " + std::to_wstring(pi.dwProcessId));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result = TRUE;
    }
    else
    {
        OutputLogToCChokka(L"  CreateProcessAsUser failed. Error: " + std::to_wstring(GetLastError()));
        result = FALSE;
    }

    CloseHandle(hDuplicatedToken);
    CloseHandle(hUserToken);

    return result;
}

// ログオン画面（winsta0\Winlogon）でDisplayRotatorを起動する関数
// 警告: この関数はセキュアデスクトップでプロセスを実行するため、セキュリティリスクがあります
BOOL LaunchProcessOnLogonScreen()
{
    HANDLE hToken = NULL;
    HANDLE hDuplicatedToken = NULL;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL result = FALSE;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    // 重要: ログオン画面のデスクトップを指定
    si.lpDesktop = (LPWSTR)L"winsta0\\Winlogon";

    ZeroMemory(&pi, sizeof(pi));

    OutputLogToCChokka(L"  Attempting to launch DisplayRotator on Logon screen (winsta0\\Winlogon)");

    // 現在のプロセス（サービス）のトークンを取得
    // サービスはSYSTEM権限で動作しているため、このトークンを使用する
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
    {
        OutputLogToCChokka(L"  OpenProcessToken failed. Error: " + std::to_wstring(GetLastError()));
        return FALSE;
    }

    // トークンを複製
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(
        hToken,
        MAXIMUM_ALLOWED,
        &sa,
        SecurityIdentification,
        TokenPrimary,
        &hDuplicatedToken))
    {
        OutputLogToCChokka(L"  DuplicateTokenEx failed. Error: " + std::to_wstring(GetLastError()));
        CloseHandle(hToken);
        return FALSE;
    }

    // DisplayRotator.exeのパス
    WCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath);
    wcscat_s(exePath, L"\\DisplayRotator.exe");

    // コマンドライン（回転回数を指定）
    WCHAR commandLine[MAX_PATH];
    wsprintf(commandLine, L"\"%s\" 4", exePath);

    OutputLogToCChokka(L"  Launching on Logon screen: " + std::wstring(commandLine));

    // プロセスを起動
    if (CreateProcessAsUser(
        hDuplicatedToken,
        NULL,
        commandLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS,
        NULL,
        NULL,
        &si,
        &pi))
    {
        OutputLogToCChokka(L"  CreateProcessAsUser on Logon screen succeeded. ProcessID: " + std::to_wstring(pi.dwProcessId));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result = TRUE;
    }
    else
    {
        DWORD error = GetLastError();
        OutputLogToCChokka(L"  CreateProcessAsUser on Logon screen failed. Error: " + std::to_wstring(error));

        // エラー1314は「要求された操作には、このプロセスが保持していない特権が必要です」
        // エラー5は「アクセスが拒否されました」
        if (error == 1314)
        {
            OutputLogToCChokka(L"  Error 1314: Insufficient privileges. Service may need SE_TCB_NAME privilege.");
        }
        else if (error == 5)
        {
            OutputLogToCChokka(L"  Error 5: Access denied. Logon desktop may be restricted.");
        }

        result = FALSE;
    }

    CloseHandle(hDuplicatedToken);
    CloseHandle(hToken);

    return result;
}

