// Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <wa_out.h>

static bool versionIsSix(HMODULE module)
{
    char path[MAX_PATH];
    if (!module || !GetModuleFileNameA(module, path, MAX_PATH)) return false;
    DWORD unused = 0;
    const DWORD size = GetFileVersionInfoSizeA(path, &unused);
    if (!size) return false;
    std::vector<BYTE> bytes(size);
    if (!GetFileVersionInfoA(path, 0, size, bytes.data())) return false;
    VS_FIXEDFILEINFO* info = NULL;
    UINT length = 0;
    if (!VerQueryValueA(bytes.data(), "\\", reinterpret_cast<void**>(&info), &length) ||
        length < sizeof(*info)) return false;
    return info->dwFileVersionMS == MAKELONG(0, 6) && info->dwFileVersionLS == 0 &&
        info->dwProductVersionMS == MAKELONG(0, 6) && info->dwProductVersionLS == 0;
}

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    HMODULE plugin = LoadLibraryA(argv[1]);
    if (!plugin) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 2;
    }
    typedef Out_Module* (*Entry)();
    Entry entry = reinterpret_cast<Entry>(GetProcAddress(plugin, "winampGetOutModule"));
    if (!entry) return 3;
    Out_Module* module = entry();
    if (!module || module->version != OUT_VER || !module->description ||
        !module->Init || !module->Quit || !module->Open || !module->Close ||
        !module->Write || !module->CanWrite || !module->IsPlaying ||
        !module->Pause || !module->SetVolume || !module->SetPan ||
        !module->Flush || !module->GetOutputTime || !module->GetWrittenTime)
        return 4;
    if (!std::strstr(module->description, "(6.0)") || !versionIsSix(plugin) ||
        !versionIsSix(GetModuleHandleA("apxCore.dll"))) return 5;
    std::printf("Loaded Winamp output interface: %s\n", module->description);
    // Playback callbacks need a real Winamp window and are tested on the user's PC.
    FreeLibrary(plugin);
    return 0;
}
