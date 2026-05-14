#include <windows.h>
#include <string>
#include <fstream>

// ログファイルパス
#define LOG_FILE_PATH L"C:\\temp\\DisplayRotator.log"

// ログ出力関数
void WriteLog(const std::wstring& message)
{
    std::wofstream logFile;
    logFile.open(LOG_FILE_PATH, std::ios::app);
    if (logFile.is_open())
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        wchar_t timeStr[256];
        wsprintf(timeStr, L"[%04d/%02d/%02d %02d:%02d:%02d.%03d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        logFile << timeStr << message << std::endl;
        logFile.close();
    }
}

// 画面を1回90度回転させる関数
BOOL RotateDisplayOnce()
{
    DISPLAY_DEVICE displayDevice;
    ZeroMemory(&displayDevice, sizeof(displayDevice));
    displayDevice.cb = sizeof(displayDevice);

    // プライマリディスプレイを探す
    DWORD deviceNum = 0;
    WCHAR primaryDeviceName[32] = { 0 };
    BOOL foundPrimary = FALSE;

    while (EnumDisplayDevices(NULL, deviceNum, &displayDevice, 0))
    {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            wcscpy_s(primaryDeviceName, displayDevice.DeviceName);
            foundPrimary = TRUE;
            WriteLog(L"Found primary display: " + std::wstring(primaryDeviceName));
            break;
        }
        deviceNum++;
    }

    if (!foundPrimary)
    {
        WriteLog(L"Failed to find primary display");
        return FALSE;
    }

    // 現在の設定を取得
    DEVMODE currentMode;
    ZeroMemory(&currentMode, sizeof(currentMode));
    currentMode.dmSize = sizeof(currentMode);

    if (!EnumDisplaySettings(primaryDeviceName, ENUM_CURRENT_SETTINGS, &currentMode))
    {
        WriteLog(L"Failed to get current display settings. Error: " + std::to_wstring(GetLastError()));
        return FALSE;
    }

    // 現在の向きを取得
    DWORD currentOrientation = currentMode.dmDisplayOrientation;
    WriteLog(L"Current orientation: " + std::to_wstring(currentOrientation) +
        L", Width: " + std::to_wstring(currentMode.dmPelsWidth) +
        L", Height: " + std::to_wstring(currentMode.dmPelsHeight));

    // 90度回転させる
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

    WriteLog(L"Rotating display from " + std::to_wstring(currentOrientation) +
        L" to " + std::to_wstring(newOrientation));

    // 新しいモードを設定
    DEVMODE targetMode = currentMode;
    targetMode.dmDisplayOrientation = newOrientation;

    // 90度または270度回転の場合は、幅と高さを入れ替える
    if ((currentOrientation == DMDO_DEFAULT || currentOrientation == DMDO_180) &&
        (newOrientation == DMDO_90 || newOrientation == DMDO_270))
    {
        // 横向き→縦向き
        DWORD temp = targetMode.dmPelsWidth;
        targetMode.dmPelsWidth = targetMode.dmPelsHeight;
        targetMode.dmPelsHeight = temp;
        WriteLog(L"Swapping dimensions (landscape to portrait): " +
            std::to_wstring(targetMode.dmPelsWidth) + L"x" + std::to_wstring(targetMode.dmPelsHeight));
    }
    else if ((currentOrientation == DMDO_90 || currentOrientation == DMDO_270) &&
        (newOrientation == DMDO_DEFAULT || newOrientation == DMDO_180))
    {
        // 縦向き→横向き
        DWORD temp = targetMode.dmPelsWidth;
        targetMode.dmPelsWidth = targetMode.dmPelsHeight;
        targetMode.dmPelsHeight = temp;
        WriteLog(L"Swapping dimensions (portrait to landscape): " +
            std::to_wstring(targetMode.dmPelsWidth) + L"x" + std::to_wstring(targetMode.dmPelsHeight));
    }

    targetMode.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;

    // ディスプレイ設定を変更
    LONG result = ChangeDisplaySettingsEx(primaryDeviceName, &targetMode, NULL, 0, NULL);

    switch (result)
    {
    case DISP_CHANGE_SUCCESSFUL:
        WriteLog(L"Display rotation successful");
        return TRUE;
    case DISP_CHANGE_RESTART:
        WriteLog(L"Display rotation requires restart");
        return FALSE;
    case DISP_CHANGE_FAILED:
        WriteLog(L"Display rotation failed");
        return FALSE;
    case DISP_CHANGE_BADMODE:
        WriteLog(L"Display rotation failed: Bad mode (unsupported orientation)");
        return FALSE;
    case DISP_CHANGE_NOTUPDATED:
        WriteLog(L"Display rotation failed: Not updated");
        return FALSE;
    case DISP_CHANGE_BADFLAGS:
        WriteLog(L"Display rotation failed: Bad flags");
        return FALSE;
    case DISP_CHANGE_BADPARAM:
        WriteLog(L"Display rotation failed: Bad parameter");
        return FALSE;
    default:
        WriteLog(L"Display rotation unknown result: " + std::to_wstring(result));
        return FALSE;
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    WriteLog(L"DisplayRotator started (v2)");

    // コマンドライン引数を確認（回転回数）
    int rotationCount = 4; // デフォルトは4回

    if (lpCmdLine && wcslen(lpCmdLine) > 0)
    {
        rotationCount = _wtoi(lpCmdLine);
        if (rotationCount < 1 || rotationCount > 100)
        {
            rotationCount = 4; // 範囲外の場合はデフォルト
        }
    }

    WriteLog(L"Starting display rotation sequence (" + std::to_wstring(rotationCount) + L" times, 5 seconds interval)");

    for (int i = 0; i < rotationCount; i++)
    {
        WriteLog(L"Rotation attempt " + std::to_wstring(i + 1) + L" of " + std::to_wstring(rotationCount));

        RotateDisplayOnce();

        // 最後の回転後は待機しない
        if (i < rotationCount - 1)
        {
            WriteLog(L"Waiting 5 seconds before next rotation...");
            Sleep(1000); // 5秒待機
        }
    }

    WriteLog(L"Display rotation sequence completed");

    return 0;
}
