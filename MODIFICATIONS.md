# Changes from upstream 5.6

Artwork provider safety, 2026-11-16:

- Disable artwork retrieval by default (`EnableArtwork=0`) following a host crash
  in the artwork path. Keep audio and text metadata enabled; preserve opt-in
  `EnableArtwork=1` across settings saves.
- Check each raw artwork provider's IsMine before calling GetAlbumArtData.
- Acquire Wasabi interfaces without a global lock and release them through their
  owning factory. Do not pass factory pointers to api_service::service_release.

Winamp file-association startup safety, 2026-11-16:

- Do not query file tags/artwork synchronously from the output Open callback,
  which can run while Winamp's input decoder is still initializing.
- Refresh metadata on the host UI thread after playback begins, using the window
  subclass and a bounded timer; cancel the timer on close/quit. Prevent recursive
  refresh when metadata services trigger another title notification.
- Schedule that timer through a posted window message so `SetTimer` executes
  on the thread owning the Winamp window.
- Reject IPC pointer sentinel 1 and copy the host-owned filename before further
  IPC calls. Added fake-host tests for startup, worker calls and pointer lifetime.

Reduced start latency, 2026-11-16:

- Connection readiness is checked every 25 ms instead of 250 ms. The 3-second
  dialog delay, approximately 5-second retry and progress update rate are kept.
- Made the original 1750 ms RTP synchronization delay a CMake build option.
  CI also builds a separate experimental 750 ms variant. Queue and retransmit
  capacity is kept; the sender already sends its first data without filling it.
- Log the receiver-reported Audio-Latency and time of first audio packet.
- Both variants retain diagnostic logging and run the startup/load checks.

Playback diagnostics, 2026-11-16:

- Added a bounded local `%TEMP%\out_apx.log` file with timestamps and thread IDs.
- Logged the Winamp initialization/open stages and RAOP setup stages.
- Replaced raw RTSP request/response dumps with method/status summaries so the
  diagnostic file omits authentication headers, stream keys and metadata bodies.
- Added a hardware-independent session startup regression test. It reproduced a
  failure caused by Poco 1.9's lazy default UDP socket creation. The RAOP engine
  now explicitly creates IPv4 sockets before setting socket options, and uses
  `WSAIoctl` for `SIO_UDP_CONNRESET` instead of `ioctlsocket`.

2026-11-16:

- Imported the original source tree, retaining copyright and license notices.
- Converted text files from their original encoding/CRLF to UTF-8/LF.
- Replaced `std::tr1::` with the corresponding C++11 `std::` names in C++ headers
  and sources; C++14 preserves the original `std::auto_ptr` code.
- Updated RSA public-key initialization in `RAOPEngine.cpp` to the opaque-key
  OpenSSL API (`RSA_set0_key`), with allocation-failure cleanup.
- Replaced MFC's `afxres.h` with Windows SDK `windows.h` (with an `IDC_STATIC` fallback) in resource scripts,
  selected UTF-8 resource code pages and made icon paths relative to the root.
- Added a CMake Winamp/core build, pinned dependency bootstrap, Windows x86
  GitHub Actions workflow and DLL/interface loading check.
- Selected the modern CRT's `lrint`/`lrintf` for libsamplerate, avoiding its
  legacy fallback definitions that conflict with MSVC 2022 intrinsics.
- Cached installed dependencies before plugin compilation and enabled automatic
  cancellation of outdated builds on the same branch.
- Packaged the core next to `winamp.exe` and the output module under `Plugins/`
  so installation does not require the upstream installer's system PATH change.
- Selected GPL-3.0-or-later for the combined build with Apache-2.0 dependencies.

Foobar2000 and XMPlay source is retained but is not built by the new workflow.
No audio/network playback behavior changes are intended by these build changes.

- Removed an invalid constructor qualifier and const-qualified container element
  types rejected by current MSVC; renamed the NTP maximum constant to avoid a
  collision with the standard `UINT32_MAX` macro.
