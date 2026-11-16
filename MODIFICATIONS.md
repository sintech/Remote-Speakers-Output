# Changes from upstream 5.6

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
