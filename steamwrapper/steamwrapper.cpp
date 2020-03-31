// Copyright (c) 2020, Loïc Hoguin <essen@ninenines.eu>
// 
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
// 
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <windows.h>
#include <tchar.h>
#include <string>
#include <fstream>
#include <streambuf>
#include <limits>

#include <steam_api.h>

int loop(wchar_t* lpFile, HANDLE hProcess, HANDLE hFileChange)
{
    DWORD waitRet;
    HANDLE handles[2] = { hProcess, hFileChange };

    while (true) {
        waitRet = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitRet == WAIT_OBJECT_0) {
            FindCloseChangeNotification(hFileChange);

            return 0;
        }
        else if (waitRet == WAIT_OBJECT_0 + 1) {
//            OutputDebugString(L"[S_WRAP] Reading achievements file\n");

            std::ifstream t(lpFile);
            std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

//            OutputDebugString(L"[S_WRAP] Parsing achievements file\n");

            size_t pos = 0;
            while (pos != std::string::npos) {
                pos = str.find(", ");
                std::string ach = str.substr(0, pos);

                if (!ach.empty()) {
//                    OutputDebugString(L"[S_WRAP] Setting achievement ");
//                    OutputDebugStringA(ach.c_str());
//                    OutputDebugString(L"\n");

                    SteamUserStats()->SetAchievement(ach.c_str());
                }

                if (pos != std::string::npos) {
                    str = str.substr(pos + 2);
                }
            }

            SteamUserStats()->StoreStats();

            FindNextChangeNotification(hFileChange);

            continue;
        }
        else {
            FindCloseChangeNotification(hFileChange);

            return 4;
        }
    }

//    OutputDebugString(L"[S_WRAP] Unreachable clause reached\n");
    return 3;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, INT)
{
    wchar_t lpIni[1024], lpGame[32], lpDirectory[256], lpFile[256];
    wchar_t cmdLine[34];
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    HANDLE hFileChange;

    // The functions to read .ini files must specify the filename as a full path.
    GetFullPathName(L"steamwrapper.ini", 1024, lpIni, NULL);

    if (!GetPrivateProfileString(L"steamwrapper", L"game", NULL, lpGame, 31, lpIni)) {
        return 11;
    }

    if (!GetPrivateProfileString(L"steamwrapper", L"directory", L"saveGames", lpDirectory, 255, lpIni)) {
        return 12;
    }

    if (!GetPrivateProfileString(L"steamwrapper", L"file", L"saveGames\\achievements.txt", lpFile, 255, lpIni)) {
        return 13;
    }

    // We must quote the executable name to avoid errors.
    memset(cmdLine, '\0', 34);
    cmdLine[0] = '"';
    wcscpy_s(cmdLine + 1, 31, lpGame);
    cmdLine[wcsnlen_s(cmdLine, 33)] = '"';

    // The "saveGames" directory must exist in order for us to monitor it.
    CreateDirectory(lpDirectory, NULL);

    hFileChange = FindFirstChangeNotification(lpDirectory, false, FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &info, &processInfo)) {
//        OutputDebugString(L"[S_WRAP] Game started\n");

        if (SteamAPI_Init()) {
            return loop(lpFile, processInfo.hProcess, hFileChange);
        }

//        OutputDebugString(L"[S_WRAP] SteamAPI initialization failure\n");
        return 2;
    }

//    OutputDebugString(L"[S_WRAP] Game failed to start\n");
    return 1;
}
