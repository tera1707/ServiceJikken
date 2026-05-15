#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <tuple>
#include <optional>

// セッションのユーザー名を取得する関数
std::wstring GetSessionUserName(DWORD sessionId);

// セッションの状態を取得する関数
std::wstring GetSessionState(DWORD sessionId);

// 現在ログイン中のユーザーを確認して情報を返す関数
// 戻り値: std::vector<std::tuple<ユーザー名(optional), セッションID, 状態>>
// ユーザーがいないセッションはユーザー名部分がstd::nullopt
std::vector<std::tuple<std::optional<std::wstring>, DWORD, std::wstring>> GetLoggedInUsers();
