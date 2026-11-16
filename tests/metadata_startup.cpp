// Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
#include "WinampPlayer.h"
#include <wa_ipc.h>
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <OptionsUtils.h>
#include <wasabi/api_service.h>
#include <wasabi/services.h>
#include <wasabi/api_albumart.h>
#include <atlbase.h>
#include <wincodec.h>
#include <wasabi/api_memmgr.h>
#include <wasabi/svc_albumArtProvider.h>
#include <wasabi/waservicefactory.h>

static int invalidArtworkCalls = 0, globalLocks = 0, managerReleases = 0;
static int serviceQueries = 0, freedBuffers = 0;
static bool useFallback = false;
static ARGB32* fallbackPixels = NULL;
static int unexpectedWriterQueries = 0;

class ArtProvider : public svc_albumArtProvider {
public:
    bool mine;
    int requests = 0;
    explicit ArtProvider(bool value) : mine(value) {}
    int _dispatch(int msg, void* result, void** args, int) override {
        if (msg == SVC_ALBUMARTPROVIDER_ISMINE) {
            *static_cast<bool*>(result) = mine;
            return 1;
        }
        if (msg == SVC_ALBUMARTPROVIDER_GETALBUMARTDATA) {
            ++requests;
            if (useFallback) { *static_cast<int*>(result) = ALBUMARTPROVIDER_FAILURE; return 1; }
            if (!mine) { ++invalidArtworkCalls; *static_cast<int*>(result) = ALBUMARTPROVIDER_FAILURE; return 1; }
            void* bytes = std::malloc(4);
            std::memcpy(bytes, "JPEG", 4);
            wchar_t* mime = static_cast<wchar_t*>(std::malloc(sizeof(L"jpeg")));
            std::memcpy(mime, L"jpeg", sizeof(L"jpeg"));
            **reinterpret_cast<void***>(args[2]) = bytes;
            **reinterpret_cast<size_t**>(args[3]) = 4;
            **reinterpret_cast<wchar_t***>(args[4]) = mime;
            *static_cast<int*>(result) = ALBUMARTPROVIDER_SUCCESS;
            return 1;
        }
        return 0;
    }
};

class MemoryManager : public api_memmgr {
public:
    int _dispatch(int msg, void*, void** args, int) override {
        if (msg != API_MEMMGR_SYSFREE) return 0;
        std::free(*static_cast<void**>(args[0]));
        ++freedBuffers;
        return 1;
    }
};

class Factory : public waServiceFactory {
public:
    Dispatchable* value;
    int releases = 0;
    explicit Factory(Dispatchable* item) : value(item) {}
    int _dispatch(int msg, void* result, void** args, int) override {
        if (msg == WASERVICEFACTORY_GETINTERFACE) {
            if (*static_cast<int*>(args[0])) ++globalLocks;
            *static_cast<void**>(result) = value;
            return 1;
        }
        if (msg == WASERVICEFACTORY_RELEASEINTERFACE) {
            if (*static_cast<void**>(args[0]) != value) ++invalidArtworkCalls;
            ++releases;
            *static_cast<int*>(result) = 1;
            return 1;
        }
        return 0;
    }
};

class AlbumArt : public api_albumart {
public:
    int _dispatch(int msg, void* result, void** args, int) override {
        if (msg != API_ALBUMART_GETALBUMART) return 0;
        **static_cast<int**>(args[2]) = 2;
        **static_cast<int**>(args[3]) = 2;
        fallbackPixels = static_cast<ARGB32*>(std::malloc(4 * sizeof(ARGB32)));
        for (int i = 0; i < 4; ++i) fallbackPixels[i] = 0xFF123456;
        **static_cast<ARGB32***>(args[4]) = fallbackPixels;
        *static_cast<int*>(result) = ALBUMART_SUCCESS;
        return 1;
    }
};
static AlbumArt albumArt;
static Factory albumFactory(&albumArt);

// Decode the actual system-encoded JPEG and check channel order, not just a mock.
static bool verifyJPEG(const buffer_t& jpeg)
{
    if (jpeg.size() < 4 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) return false;
    HRESULT apartment = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment)) return false;
    bool valid = false;
    {
        CComPtr<IWICImagingFactory> factory;
        CComPtr<IWICStream> stream;
        CComPtr<IWICBitmapDecoder> decoder;
        CComPtr<IWICBitmapFrameDecode> frame;
        CComPtr<IWICFormatConverter> converter;
        UINT width = 0, height = 0;
        BYTE pixels[12] = {};
        valid = SUCCEEDED(factory.CoCreateInstance(CLSID_WICImagingFactory)) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(jpeg.data()), static_cast<DWORD>(jpeg.size()))) &&
            SUCCEEDED(factory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder)) &&
            SUCCEEDED(decoder->GetFrame(0, &frame)) &&
            SUCCEEDED(frame->GetSize(&width, &height)) && width == 2 && height == 2 &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
            SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat24bppRGB, WICBitmapDitherTypeNone,
                NULL, 0.0, WICBitmapPaletteTypeCustom)) &&
            SUCCEEDED(converter->CopyPixels(NULL, 6, sizeof(pixels), pixels));
        for (int i = 0; valid && i < 4; ++i) {
            valid = std::abs(int(pixels[i*3]) - 0x12) <= 8 &&
                std::abs(int(pixels[i*3+1]) - 0x34) <= 8 &&
                std::abs(int(pixels[i*3+2]) - 0x56) <= 8;
        }
    }
    CoUninitialize();
    return valid;
}

static ArtProvider foreignProvider(false), matchingProvider(true);
static MemoryManager memoryManager;
static Factory foreignFactory(&foreignProvider), matchingFactory(&matchingProvider), memoryFactory(&memoryManager);

class Services : public api_service {
public:
    int _dispatch(int msg, void* result, void** args, int) override {
        if (msg == API_SERVICE_SERVICE_GETSERVICEBYGUID) {
            *static_cast<waServiceFactory**>(result) =
                IsEqualGUID(*static_cast<GUID*>(args[0]), memMgrApiServiceGuid) ? &memoryFactory :
                IsEqualGUID(*static_cast<GUID*>(args[0]), albumArtGUID) ? &albumFactory : NULL;
            return 1;
        }
        if (msg == API_SERVICE_SERVICE_GETNUMSERVICES) {
            if (*static_cast<FOURCC*>(args[0]) == WaSvc::IMAGEWRITER) {
                ++unexpectedWriterQueries;
                *static_cast<int*>(result) = 0;
            } else *static_cast<int*>(result) = 2;
            return 1;
        }
        if (msg == API_SERVICE_SERVICE_ENUMSERVICE) {
            *static_cast<waServiceFactory**>(result) =
                *static_cast<size_t*>(args[1]) == 0 ? &foreignFactory : &matchingFactory;
            return 1;
        }
        if (msg == API_SERVICE_SERVICE_RELEASE) {
            ++managerReleases; // factory pointers must never be passed here
            *static_cast<int*>(result) = 0;
            return 1;
        }
        return 0;
    }
};
static Services services;

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
    case IPC_GET_API_SERVICE: ++serviceQueries; return reinterpret_cast<LRESULT>(&services);
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
    if (serviceQueries != 0 || !artwork.empty()) return 8; // default-off must not touch Wasabi
    wcscpy_s(borrowedPath, L"startup.mp3");
    if (player.getPlaybackMetadata(title, album, artist, artwork, type, playlist, true, true) != 42000)
        return 9;
    if (artwork.size() != 4 || type != "image/jpeg" || foreignProvider.requests != 0 ||
        matchingProvider.requests != 1 || invalidArtworkCalls != 0 || globalLocks != 0 ||
        managerReleases != 0 || foreignFactory.releases != 1 || matchingFactory.releases != 1 || freedBuffers != 2)
        return 10;
    useFallback = true;
    wcscpy_s(borrowedPath, L"startup.mp3");
    player.getPlaybackMetadata(title, album, artist, artwork, type, playlist, true, true);
    if (unexpectedWriterQueries != 0 || type != "image/jpeg" || !verifyJPEG(artwork) ||
        invalidArtworkCalls != 0 || freedBuffers != 3 || albumFactory.releases != 1 ||
        globalLocks != 0 || managerReleases != 0) return 13;
    char folder[MAX_PATH], ini[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, folder) || !GetTempFileNameA(folder, "apx", 0, ini)) return 11;
    WritePrivateProfileStringA("Remote Speakers output", "EnableArtwork", "1", ini);
    OptionsUtils::loadOptions(ini);
    OptionsUtils::saveOptions(ini);
    const bool preserved = GetPrivateProfileIntA("Remote Speakers output", "EnableArtwork", 0, ini) == 1;
    DeleteFileA(ini);
    if (!preserved) return 12;
    DestroyWindow(window);
    std::puts("Startup, IPC ownership, raw artwork, real WIC JPEG dimensions/colors and settings preservation passed.");
    return 0;
}
