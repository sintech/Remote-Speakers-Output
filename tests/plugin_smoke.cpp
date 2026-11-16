// Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <wa_out.h>

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
    std::printf("Loaded Winamp output interface: %s\n", module->description);
    // Playback callbacks need a real Winamp window and are tested on the user's PC.
    FreeLibrary(plugin);
    return 0;
}
