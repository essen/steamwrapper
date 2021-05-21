// steamwrapper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <tchar.h>
#include <atlconv.h>
#include <string>
#include <fstream>
#include <streambuf>
#include <limits>

#include <steam_api.h>
#include <galaxy/GalaxyApi.h>

// GOG Galaxy specific functions.
//
// Because the GOG Galaxy API uses asynchronous calls all over the place
// we must setup listeners and wait for them to indicate success or failure
// before continuing. These functions basically help make the calls synchronous.

static int sign_in_result = 0;

int galaxy_wait_for_sign_in()
{
    while (sign_in_result == 0) {
        galaxy::api::ProcessData();
        Sleep(1);
    }

    return sign_in_result;
}

class AuthListener : public galaxy::api::GlobalAuthListener
{
public:
    virtual void OnAuthSuccess() override {
        sign_in_result = 1;
    };

    virtual void OnAuthFailure(FailureReason reason) override {
        sign_in_result = -1;
    };

    virtual void OnAuthLost() override {
        sign_in_result = -2;
    };
};

static int retrieve_result = 0;

int galaxy_wait_for_retrieve()
{
    while (retrieve_result == 0) {
        galaxy::api::ProcessData();
        Sleep(1);
    }

    return retrieve_result;
}

class StatsAndAchievementsRetrieveListener : public galaxy::api::GlobalUserStatsAndAchievementsRetrieveListener
{
public:
    virtual void OnUserStatsAndAchievementsRetrieveSuccess(galaxy::api::GalaxyID userID) override {
        retrieve_result = 1;
    };

    virtual void OnUserStatsAndAchievementsRetrieveFailure(galaxy::api::GalaxyID userID, FailureReason reason) override {
        retrieve_result = -1;
    };
};

static int store_result = 0;

int galaxy_wait_for_store()
{
    while (store_result == 0) {
        galaxy::api::ProcessData();
        Sleep(1);
    }

    return store_result;
}

class StatsAndAchievementsStoreListener : public galaxy::api::GlobalStatsAndAchievementsStoreListener
{
public:
    virtual void OnUserStatsAndAchievementsStoreSuccess() override {
        store_result = 1;
    };

    virtual void OnUserStatsAndAchievementsStoreFailure(FailureReason failureReason) override {
        store_result = -1;
    };
};

// End of GOG Galaxy specific functions.

enum class GamePlatform {
    Steam,
    GOG
};

int loop(GamePlatform platform, wchar_t* lpFile, HANDLE hProcess, HANDLE hFileChange)
{
    DWORD waitRet;
    HANDLE handles[2] = { hProcess, hFileChange };

    while (true) {
        waitRet = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitRet == WAIT_OBJECT_0) {
            FindCloseChangeNotification(hFileChange);

            return 0;
        } else if (waitRet == WAIT_OBJECT_0 + 1) {
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

                    switch (platform) {
                    case GamePlatform::Steam:
                        SteamUserStats()->SetAchievement(ach.c_str());
                        break;

                    case GamePlatform::GOG:
                        galaxy::api::Stats()->SetAchievement(ach.c_str());
                        break;
                    }
                }

                if (pos != std::string::npos) {
                    str = str.substr(pos + 2);
                }
            }

            switch (platform) {
            case GamePlatform::Steam:
                SteamUserStats()->StoreStats();
                break;

            case GamePlatform::GOG:
                StatsAndAchievementsStoreListener store_listener;
                galaxy::api::Stats()->StoreStatsAndAchievements();
                galaxy_wait_for_store();
                break;
            }

            FindNextChangeNotification(hFileChange);

            continue;
        } else {
            FindCloseChangeNotification(hFileChange);

            return 4;
        }
    }

//    OutputDebugString(L"[S_WRAP] Unreachable clause reached\n");
    return 3;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, INT)
{
    wchar_t lpIni[1024], lpPlatform[16], lpGame[32], lpDirectory[256], lpFile[256], lpClientId[32], lpClientSecret[128];
    wchar_t cmdLine[34];
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    HANDLE hFileChange;
    GamePlatform platform;

    // The functions to read .ini files must specify the filename as a full path.
    GetFullPathName(L"steamwrapper.ini", 1024, lpIni, NULL);

    // We default to Steam when there are no "platform" value or when it is unknown.
    if (GetPrivateProfileString(L"steamwrapper", L"platform", L"saveGames\\achievements.txt", lpPlatform, 15, lpIni)) {
        if (wcscmp(lpPlatform, L"steam") == 0) {
            platform = GamePlatform::Steam;
        } else if (wcscmp(lpPlatform, L"gog") == 0) {
            platform = GamePlatform::GOG;
        }
        else {
            platform = GamePlatform::Steam;
        }
    } else {
        platform = GamePlatform::Steam;
    }

    if (!GetPrivateProfileString(L"steamwrapper", L"game", NULL, lpGame, 31, lpIni)) {
        return 12;
    }

    if (!GetPrivateProfileString(L"steamwrapper", L"directory", L"saveGames", lpDirectory, 255, lpIni)) {
        return 13;
    }

    if (!GetPrivateProfileString(L"steamwrapper", L"file", L"saveGames\\achievements.txt", lpFile, 255, lpIni)) {
        return 14;
    }

    // client_id is only used by GOG.
    if (!GetPrivateProfileString(L"steamwrapper", L"client_id", L"saveGames\\achievements.txt", lpClientId, 31, lpIni)) {
        lpClientId[0] = '\0';
    }

    // client_secret is only used by GOG.
    if (!GetPrivateProfileString(L"steamwrapper", L"client_secret", L"saveGames\\achievements.txt", lpClientSecret, 127, lpIni)) {
        lpClientSecret[0] = '\0';
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

        switch (platform) {
        case GamePlatform::Steam:
            if (SteamAPI_Init()) {
                return loop(platform, lpFile, processInfo.hProcess, hFileChange);
            }
            break;
        case GamePlatform::GOG:
            USES_CONVERSION;
            galaxy::api::Init({ W2A(lpClientId), W2A(lpClientSecret) });

            AuthListener auth_listener;
            galaxy::api::User()->SignInGalaxy();
            if (galaxy_wait_for_sign_in() != 1) {
                return 21;
            }

            StatsAndAchievementsRetrieveListener retrieve_listener;
            galaxy::api::Stats()->RequestUserStatsAndAchievements();
            if (galaxy_wait_for_retrieve() != 1) {
                return 22;
            }

            return loop(platform, lpFile, processInfo.hProcess, hFileChange);
            break;
        }

//        OutputDebugString(L"[S_WRAP] SteamAPI initialization failure\n");
        return 2;
    }

//    OutputDebugString(L"[S_WRAP] Game failed to start\n");
    return 1;
}