#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include <shlwapi.h>
#include <vector>
#include <tuple>
#include <optional>
#include <dbt.h>
#include <initguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include "serviceJikken1.h"
#include "SessionInfo.h"

// SERVICE_ACCEPT_DEVICEEVENT が定義されていない場合の対処
#ifndef SERVICE_ACCEPT_DEVICEEVENT
#define SERVICE_ACCEPT_DEVICEEVENT 0x00000200
#endif

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Ole32.lib")

#define SVCNAME TEXT("SvcName")

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;
HDEVNOTIFY              ghDevNotify = NULL;  // デバイス通知ハンドル

DWORD WINAPI SvcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcMainLoop(DWORD, LPTSTR*);
VOID SvcEnd(DWORD, LPTSTR*);

// デバイスインターフェースパスからデバイスインスタンスIDを取得
std::wstring GetDeviceInstanceIdFromInterfacePath(const std::wstring& interfacePath)
{
    CONFIGRET cr;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN];
    DEVPROPTYPE propertyType;
    ULONG propertySize = sizeof(deviceInstanceId);

    cr = CM_Get_Device_Interface_PropertyW(
        interfacePath.c_str(),
        &DEVPKEY_Device_InstanceId,
        &propertyType,
        (PBYTE)deviceInstanceId,
        &propertySize,
        0
    );

    if (cr == CR_SUCCESS && propertyType == DEVPROP_TYPE_STRING)
    {
        return std::wstring(deviceInstanceId);
    }

    return L"";
}

// デバイスインスタンスIDからハードウェアIDを取得
std::wstring GetHardwareId(const std::wstring& deviceInstanceId)
{
    if (deviceInstanceId.empty())
        return L"";

    CONFIGRET cr;
    DEVINST devInst;

    cr = CM_Locate_DevNodeW(&devInst, (DEVINSTID_W)deviceInstanceId.c_str(), CM_LOCATE_DEVNODE_NORMAL);
    if (cr != CR_SUCCESS)
        return L"";

    WCHAR hardwareId[1024] = { 0 };
    ULONG size = sizeof(hardwareId);
    DEVPROPTYPE propertyType;

    cr = CM_Get_DevNode_PropertyW(
        devInst,
        &DEVPKEY_Device_HardwareIds,
        &propertyType,
        (PBYTE)hardwareId,
        &size,
        0
    );

    if (cr == CR_SUCCESS && propertyType == DEVPROP_TYPE_STRING_LIST)
    {
        // 最初のハードウェアIDを返す（最も具体的なもの）
        return std::wstring(hardwareId);
    }

    return L"";
}

// VID/PIDを抽出
std::wstring ExtractVidPid(const std::wstring& hardwareId)
{
    // USB\VID_046D&PID_C52B のような形式から VID と PID を抽出
    size_t vidPos = hardwareId.find(L"VID_");
    size_t pidPos = hardwareId.find(L"PID_");

    if (vidPos != std::wstring::npos && pidPos != std::wstring::npos)
    {
        std::wstring vid = hardwareId.substr(vidPos, 8); // VID_XXXX
        std::wstring pid = hardwareId.substr(pidPos, 8); // PID_YYYY
        return vid + L"&" + pid;
    }

    return L"";
}

// デバイスインスタンスIDからシリアル番号を抽出
std::wstring ExtractSerialNumber(const std::wstring& deviceInstanceId)
{
    // USB\VID_046D&PID_C52B\ABC123 の最後の部分がシリアル番号
    size_t lastSlash = deviceInstanceId.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos && lastSlash + 1 < deviceInstanceId.length())
    {
        return deviceInstanceId.substr(lastSlash + 1);
    }

    return L"";
}

// セッション情報を文字列として取得する関数
std::wstring GetSessionInfoString(DWORD sessionId)
{
    std::wstring log = L"";

    // セッションIDをログ出力
    log += L"  Session(" + std::to_wstring(sessionId) + L")";

    // ユーザー名を取得
    std::wstring userName = GetSessionUserName(sessionId);
    if (!userName.empty())
    {
        log += L"  User: " + userName;
    }
    else
    {
        log += L"  User: -";
    }

    // セッションの状態を取得
    std::wstring stateStr = GetSessionState(sessionId);
    if (!stateStr.empty())
    {
        log += L"  State: " + stateStr;
    }

    return log;
}

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
            std::wstring sessionInfo = GetSessionInfoString(sessionNotif->dwSessionId);
            OutputLogToCChokka(sessionInfo);
        }

        // 現在ログイン中のユーザーを確認してログ出力
        auto sessions = GetLoggedInUsers();
        if (!sessions.empty())
        {
            std::wstring loggedInUsers;
            for (const auto& [optUserName, sessionId, state] : sessions)
            {
                if (!loggedInUsers.empty())
                {
                    loggedInUsers += L", ";
                }

                if (optUserName.has_value())
                {
                    loggedInUsers += optUserName.value() + L"(" + std::to_wstring(sessionId) + L", " + state + L")";
                }
                else
                {
                    loggedInUsers += L"(no user)(" + std::to_wstring(sessionId) + L", " + state + L")";
                }
            }
            OutputLogToCChokka(L"  users: " + loggedInUsers);
        }
        else
        {
            OutputLogToCChokka(L"  No sessions found");
        }

        break;
    }

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    case SERVICE_CONTROL_POWEREVENT:
        switch (dwEventType)
        {
        case PBT_POWERSETTINGCHANGE:
        {
            auto data = reinterpret_cast<POWERBROADCAST_SETTING*>(lpEventData);

            if (data->PowerSetting == GUID_BATTERY_PERCENTAGE_REMAINING)
            {
                auto bat = reinterpret_cast<DWORD*>(data->Data);
                auto log = std::wstring(L" * battery remain is " + std::to_wstring(*bat));
                OutputLogToCChokka(log.c_str());
            }
            else if (data->PowerSetting == GUID_ACDC_POWER_SOURCE)
            {
                auto src = reinterpret_cast<DWORD*>(data->Data);
                std::wstring srcStr = GetMapValueOrDefault(PowSrcMap, *src, L"UNKNOWN_POWER_SOURCE");
                auto log = std::wstring(L" * power source is " + srcStr);
                OutputLogToCChokka(log.c_str());
            }
            break;
        }
        }
        break;

    case SERVICE_CONTROL_DEVICEEVENT:
    {
        // DeviceEventMapの安全なアクセス
        OutputLogToCChokka(L" " + GetMapValueOrDefault(DeviceEventMap, dwEventType, L"UNKNOWN_DEVICE_EVENT"));

        if (lpEventData != NULL)
        {
            DEV_BROADCAST_HDR* pHdr = (DEV_BROADCAST_HDR*)lpEventData;

            switch (dwEventType)
            {
            case DBT_DEVICEARRIVAL:
                OutputLogToCChokka(L"  Device connected");

                if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
                {
                    DEV_BROADCAST_VOLUME* pVol = (DEV_BROADCAST_VOLUME*)lpEventData;
                    WCHAR driveLetter = L'A';
                    DWORD mask = pVol->dbcv_unitmask;
                    while (mask)
                    {
                        if (mask & 1)
                        {
                            OutputLogToCChokka(L"    Drive: " + std::wstring(1, driveLetter) + L":");
                        }
                        driveLetter++;
                        mask >>= 1;
                    }
                }
                else if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                {
                    DEV_BROADCAST_DEVICEINTERFACE* pDevInf = (DEV_BROADCAST_DEVICEINTERFACE*)lpEventData;
                    std::wstring interfacePath = pDevInf->dbcc_name;

                    OutputLogToCChokka(L"    Device interface path: " + interfacePath);

                    // インターフェースクラスGUIDを表示
                    WCHAR guidStr[40];
                    StringFromGUID2(pDevInf->dbcc_classguid, guidStr, 40);
                    OutputLogToCChokka(L"    Interface class GUID: " + std::wstring(guidStr));

                    // デバイスインスタンスIDを取得
                    std::wstring deviceInstanceId = GetDeviceInstanceIdFromInterfacePath(interfacePath);
                    OutputLogToCChokka(L"    デバイス インスタンス パス: " + (!deviceInstanceId.empty() ? deviceInstanceId : L"empty"));

                    if (!deviceInstanceId.empty())
                    {
                        // ハードウェアIDを取得
                        std::wstring hardwareId = GetHardwareId(deviceInstanceId);
                        OutputLogToCChokka(L"    ハードウェア ID: " + (!hardwareId.empty() ? hardwareId : L"empty"));

                        if (!hardwareId.empty())
                        {
                            // VID/PIDを抽出
                            std::wstring vidPid = ExtractVidPid(hardwareId);
                            if (!vidPid.empty())
                            {
                                OutputLogToCChokka(L"    VID/PID: " + vidPid);
                            }
                        }

                        // シリアル番号を抽出
                        std::wstring serialNumber = ExtractSerialNumber(deviceInstanceId);
                        OutputLogToCChokka(L"    シリアル番号: " + (!serialNumber.empty() ? serialNumber : L"empty"));
                    }
                }
                break;

            case DBT_DEVICEREMOVECOMPLETE:
                OutputLogToCChokka(L"  Device removed");

                if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
                {
                    DEV_BROADCAST_VOLUME* pVol = (DEV_BROADCAST_VOLUME*)lpEventData;
                    WCHAR driveLetter = L'A';
                    DWORD mask = pVol->dbcv_unitmask;
                    while (mask)
                    {
                        if (mask & 1)
                        {
                            OutputLogToCChokka(L"    Drive: " + std::wstring(1, driveLetter) + L":");
                        }
                        driveLetter++;
                        mask >>= 1;
                    }
                }
                break;

            case DBT_DEVNODES_CHANGED:
                OutputLogToCChokka(L"  Device configuration changed");
                break;

            case DBT_DEVICEQUERYREMOVE:
                OutputLogToCChokka(L"  Device query remove");
                break;

            case DBT_DEVICEQUERYREMOVEFAILED:
                OutputLogToCChokka(L"  Device query remove failed");
                break;

            case DBT_DEVICEREMOVEPENDING:
                OutputLogToCChokka(L"  Device remove pending");
                break;
            }
        }
        break;
    }

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

    // デバイス通知の登録（すべてのデバイスインターフェース）
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // dbcc_classguid を指定しない場合は、すべてのデバイスインターフェースクラスを監視

    ghDevNotify = RegisterDeviceNotification(gSvcStatusHandle, &NotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!ghDevNotify)
    {
        OutputLogToCChokka(L"RegisterDeviceNotification failed");
    }

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
    auto sessions = GetLoggedInUsers();
    if (!sessions.empty())
    {
        std::wstring loggedInUsers;
        for (const auto& [optUserName, sessionId, state] : sessions)
        {
            if (!loggedInUsers.empty())
            {
                loggedInUsers += L", ";
            }

            if (optUserName.has_value())
            {
                loggedInUsers += optUserName.value() + L"(SessionID:" + std::to_wstring(sessionId) + L", State:" + state + L")";
            }
            else
            {
                loggedInUsers += L"(no user)(SessionID:" + std::to_wstring(sessionId) + L", State:" + state + L")";
            }
        }
        OutputLogToCChokka(L"  users: " + loggedInUsers);
    }
    else
    {
        OutputLogToCChokka(L"  No sessions found");
    }

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

    // デバイス通知の解除
    if (ghDevNotify)
    {
        UnregisterDeviceNotification(ghDevNotify);
        ghDevNotify = NULL;
        OutputLogToCChokka(L"UnregisterDeviceNotification succeeded");
    }

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
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_DEVICEEVENT;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}
