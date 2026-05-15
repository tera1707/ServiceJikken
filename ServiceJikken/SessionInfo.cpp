#include "SessionInfo.h"
#include <wtsapi32.h>

#pragma comment(lib, "Wtsapi32.lib")

// セッションのユーザー名を取得する関数
std::wstring GetSessionUserName(DWORD sessionId)
{
    LPWSTR pBuffer = NULL;
    DWORD bytesReturned = 0;
    std::wstring userName = L"";

    if (WTSQuerySessionInformation(
        WTS_CURRENT_SERVER_HANDLE,
        sessionId,
        WTSUserName,
        &pBuffer,
        &bytesReturned))
    {
        if (pBuffer != NULL && wcslen(pBuffer) > 0)
        {
            userName = std::wstring(pBuffer);
        }
        WTSFreeMemory(pBuffer);
    }

    return userName;
}

// セッションの状態を取得する関数
std::wstring GetSessionState(DWORD sessionId)
{
    LPWSTR pBuffer = NULL;
    DWORD bytesReturned = 0;
    std::wstring stateStr = L"";

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
        }
        WTSFreeMemory(pBuffer);
    }

    return stateStr;
}

// 現在ログイン中のユーザーを確認して情報を返す関数
// 戻り値: std::vector<std::tuple<ユーザー名(optional), セッションID, 状態>>
// ユーザーがいないセッションはユーザー名部分がstd::nullopt
std::vector<std::tuple<std::optional<std::wstring>, DWORD, std::wstring>> GetLoggedInUsers()
{
    PWTS_SESSION_INFO pSessionInfo = NULL;
    DWORD sessionCount = 0;

    std::vector<std::tuple<std::optional<std::wstring>, DWORD, std::wstring>> sessions;

    if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &sessionCount))
    {
        // エラー時は空のベクターを返す（呼び出し側でエラー処理）
        return sessions;
    }

    for (DWORD i = 0; i < sessionCount; i++)
    {
        if (pSessionInfo[i].State == WTSActive || pSessionInfo[i].State == WTSDisconnected)
        {
            std::wstring userName = GetSessionUserName(pSessionInfo[i].SessionId);
            std::wstring sessionState = GetSessionState(pSessionInfo[i].SessionId);

            // ユーザー名が空の場合はnulloptにする
            std::optional<std::wstring> optUserName;
            if (!userName.empty())
            {
                optUserName = userName;
            }

            sessions.push_back(std::make_tuple(optUserName, pSessionInfo[i].SessionId, sessionState));
        }
    }

    WTSFreeMemory(pSessionInfo);

    return sessions;
}
