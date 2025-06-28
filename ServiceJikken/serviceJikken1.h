#pragma once

// DEBUG
#include <iostream>
#include <time.h>
#include <thread>   // std::this_thread::get_id()を使うのに必要
#include <fstream>  // std::wofstreamを使うのに必要
#include <map>
#include <windows.h> // SERVICE_CONTROL_XXX マクロ用
#include <WtsApi32.h> // WTS_XXX マクロ用

// std::mapから安全に値を取得するユーティリティ関数
inline std::wstring GetMapValueOrDefault(const std::map<DWORD, std::wstring>& m, DWORD key, const std::wstring& defaultValue)
{
    auto it = m.find(key);
    if (it != m.end()) {
        return it->second;
    }
    return defaultValue;
}

// サービス状態コードをマップ化
const std::map<DWORD, std::wstring> CsMap = {
    { 0, L"INIT" },
    { SERVICE_STOPPED, L"SERVICE_STOPPED" },
    { SERVICE_START_PENDING, L"SERVICE_START_PENDING" },
    { SERVICE_STOP_PENDING, L"SERVICE_STOP_PENDING" },
    { SERVICE_RUNNING, L"SERVICE_RUNNING" },
    { SERVICE_CONTINUE_PENDING, L"SERVICE_CONTINUE_PENDING" },
    { SERVICE_PAUSE_PENDING, L"SERVICE_PAUSE_PENDING" },
    { SERVICE_PAUSED, L"SERVICE_PAUSED" }
};
// 電源ソース値をマップ化（0:PoAc, 1:PoDc, 2:PoHot）
const std::map<DWORD, std::wstring> PowSrcMap = {
    { 0, L"PoAc" },
    { 1, L"PoDc" },
    { 2, L"PoHot" }
};
// SvcCtrlTbl を std::map に置き換え
const std::map<DWORD, std::wstring> SvcCtrlMap = {
    { SERVICE_CONTROL_STOP, L"SERVICE_CONTROL_STOP" },
    { SERVICE_CONTROL_PAUSE, L"SERVICE_CONTROL_PAUSE" },
    { SERVICE_CONTROL_CONTINUE, L"SERVICE_CONTROL_CONTINUE" },
    { SERVICE_CONTROL_INTERROGATE, L"SERVICE_CONTROL_INTERROGATE" },
    { SERVICE_CONTROL_SHUTDOWN, L"SERVICE_CONTROL_SHUTDOWN" },
    { SERVICE_CONTROL_PARAMCHANGE, L"SERVICE_CONTROL_PARAMCHANGE" },
    { SERVICE_CONTROL_NETBINDADD, L"SERVICE_CONTROL_NETBINDADD" },
    { SERVICE_CONTROL_NETBINDREMOVE, L"SERVICE_CONTROL_NETBINDREMOVE" },
    { SERVICE_CONTROL_NETBINDENABLE, L"SERVICE_CONTROL_NETBINDENABLE" },
    { SERVICE_CONTROL_NETBINDDISABLE, L"SERVICE_CONTROL_NETBINDDISABLE" },
    { SERVICE_CONTROL_DEVICEEVENT, L"SERVICE_CONTROL_DEVICEEVENT" },
    { SERVICE_CONTROL_HARDWAREPROFILECHANGE, L"SERVICE_CONTROL_HARDWAREPROFILECHANGE" },
    { SERVICE_CONTROL_POWEREVENT, L"SERVICE_CONTROL_POWEREVENT" },
    { SERVICE_CONTROL_SESSIONCHANGE, L"SERVICE_CONTROL_SESSIONCHANGE" },
    { SERVICE_CONTROL_PRESHUTDOWN, L"SERVICE_CONTROL_PRESHUTDOWN" },
    { SERVICE_CONTROL_TIMECHANGE, L"SERVICE_CONTROL_TIMECHANGE" },
    { SERVICE_CONTROL_TRIGGEREVENT, L"SERVICE_CONTROL_TRIGGEREVENT" },
    { SERVICE_CONTROL_LOWRESOURCES, L"SERVICE_CONTROL_LOWRESOURCES" },
    { SERVICE_CONTROL_SYSTEMLOWRESOURCES, L"SERVICE_CONTROL_SYSTEMLOWRESOURCES" }
};
// SessionScTbl を std::map に置き換え
const std::map<DWORD, std::wstring> SessionScMap = {
    { WTS_CONSOLE_CONNECT,        L"WTS_CONSOLE_CONNECT" },
    { WTS_CONSOLE_DISCONNECT,     L"WTS_CONSOLE_DISCONNECT" },
    { WTS_REMOTE_CONNECT,         L"WTS_REMOTE_CONNECT" },
    { WTS_REMOTE_DISCONNECT,      L"WTS_REMOTE_DISCONNECT" },
    { WTS_SESSION_LOGON,          L"WTS_SESSION_LOGON" },
    { WTS_SESSION_LOGOFF,         L"WTS_SESSION_LOGOFF" },
    { WTS_SESSION_LOCK,           L"WTS_SESSION_LOCK" },
    { WTS_SESSION_UNLOCK,         L"WTS_SESSION_UNLOCK" },
    { WTS_SESSION_REMOTE_CONTROL, L"WTS_SESSION_REMOTE_CONTROL" },
    { WTS_SESSION_CREATE,         L"WTS_SESSION_CREATE" },
    { WTS_SESSION_TERMINATE,      L"WTS_SESSION_TERMINATE" }
};

// ログをCドライブ直下に残すDEBUG用関数
void OutputLogToCChokka(std::wstring txt)
{
    //FILE* fp = NULL;
    auto t = time(nullptr);
    auto tmv = tm();
    auto error = localtime_s(&tmv, &t); // ローカル時間(タイムゾーンに合わせた時間)を取得

    wchar_t buf[256] = { 0 };
    wcsftime(buf, 256, L"%Y/%m/%d %H:%M:%S ", &tmv);

    // 現在のスレッドIDを出力
    auto thId = std::this_thread::get_id();

    // ログ出力
    std::wstring logtxt = buf + txt;

    // ファイルを開く(なければ作成)
    // C直下のファイルに書くにはexe実行時に管理者権限にする必要アリ
    std::wofstream ofs(L"C:\\mylog.log", std::ios::app);
    if (!ofs)
    {
        return;
    }
    // 現在時刻とスレッドIDを付けたログをファイルに書き込み
    ofs << thId << L"  " << logtxt.c_str() << std::endl;
    std::wcout << thId << L"  " << logtxt.c_str() << std::endl;
    // ファイル閉じる
    ofs.close();
}

