// Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
#include "WinampPlayer.h"
#include <wa_ipc.h>
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <thread>

static int state = 0;
static int ipcCalls = 0;
static int fileInfoCalls = 0;
static bool hasFile = false;
static bool ownedPathStayedValid = true;
static wchar_t borrowedPath[64] = L"startup.mp3";

static LRESULT CALLBACK hostProc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    if (message != WM_WA_IPC) return DefWindowProcW(window, message, wp, lp);
    ++ipcCalls;
    switch (lp) {
    case IPC_ISPLAYING: return state;
    case IPC_GET_PLAYING_FILENAME: return hasFile ? reinterpret_cast<LRESULT>(borrowedPath) : 1;
    case IPC_GET_PLAYING_TITLE: return 1; // unsupported/not-yet-ready pointer sentinel
    case IPC_GET_API_SERVICE: return 1;
    case IPC_GETOUTPUTTIME: return 42000;
    case IPC_GETLISTLENGTH: return 0;
    case IPC_GET_BASIC_FILE_INFOW: {
        ++fileInfoCalls;
        auto info = reinterpret_cast<basicFileInfoStructW*>(wp);
        ownedPathStayedValid = ownedPathStayedValid && !std::wcscmp(info->filename, L"startup.mp3");
        info->length = 42;
        wcscpy_s(borrowedPath, L"changed.mp3"); // invalidate the borrowed contents on another IPC
        return 1;
    }
    case IPC_GET_EXTENDED_FILE_INFOW: {
        ++fileInfoCalls;
        auto info = reinterpret_cast<extendedFileInfoStructW*>(wp);
        ownedPathStayedValid = ownedPathStayedValid && !std::wcscmp(info->filename, L"startup.mp3");
        if (!std::wcscmp(info->metadata, L"Title"))
            wcscpy_s(info->ret, info->retlen, L"Startup test");
        return 1;
    }
    default: return 0;
    }
}

int main()
{
    WNDCLASSW cls = {};
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpfnWndProc = hostProc;
    cls.lpszClassName = L"ApxMetadataStartupTest";
    if (!RegisterClassW(&cls)) return 1;
    HWND window = CreateWindowW(cls.lpszClassName, L"", 0, 0, 0, 1, 1,
        NULL, NULL, cls.hInstance, NULL);
    if (!window) return 2;
    WinampPlayer player(window);
    std::string title, album, artist, type;
    buffer_t artwork;
    shorts_t playlist;
    auto read = [&](bool allow) {
        return player.getPlaybackMetadata(title, album, artist, artwork, type, playlist, allow);
    };
    if (read(false) != 0 || ipcCalls != 0) return 3;
    std::thread worker([&] { read(true); });
    worker.join();
    if (ipcCalls != 0) return 4; // worker must never call Winamp metadata services
    if (read(true) != 0 || fileInfoCalls != 0) return 5; // host still starting
    state = 1;
    if (read(true) != 42000 || !title.empty() || fileInfoCalls != 0) return 6;
    hasFile = true;
    if (read(true) != 42000 || title != "Startup test" || !ownedPathStayedValid) return 7;
    DestroyWindow(window);
    std::puts("Startup deferral, UI-thread gating, IPC sentinels and owned filename passed.");
    return 0;
}
