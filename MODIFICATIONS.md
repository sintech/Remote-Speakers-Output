# Changes from upstream 5.6

Configurable playback sync delay, 2026-11-16:

- Add a numeric Playback delay (ms) field to the existing options dialog, range
  100-2000. Validate before Apply, include it in dirty detection, and preserve
  the controls' placement when the host stretches the dialog.
- Persist SyncLatencyMs with other options; invalid INI values use the build's
  default (1750 ms normal, 750 ms low-latency). Both artifacts support the range.
- Save Winamp settings on Apply. Reopen retained receiver connections on the
  next playback open when delay changes, including immediate Stop/Play within
  the existing eight-second connection reuse window.
- Capture delay during RAOP session reinitialization and use it for sync RTP
  timestamps. Apply changes after Stop/Play, keeping an active session stable.
- Test settings round trips, invalid settings, session initialization at both
  boundaries and rejection of delays outside the range.

System JPEG encoding, 2026-11-16:

- Keep Winamp's raw providers and decoded album-art API. Encode decoded pixels
  with the Microsoft WIC JPEG encoder at quality 80 instead of Winamp's writer.
  Convert ARGB32 to 24-bit BGR scanlines; balance COM initialization and release
  all COM objects. Log HRESULT failures and return empty artwork on codec errors.
- In the supplied Winamp 5.9.2 build 10042 jpeg.w5s, writer RVA 0x2e80 sets
  input_components=4 and JCS_RGB=2 (RVA 0x2fa5/0x2fd4), while the JPEG color
  converter's RGB branch requires 3 components (RVA 0x195ab). Its fatal handler
  calls exit(1) at RVA 0x3338. The external Winamp component is not modified.
- Test real JPEG encoding/decoding, dimensions and RGB channel order, retain raw
  provider/ownership checks, and assert Winamp image writers are never queried.
- WIC is built into Vista and later. This does not certify older Windows support
  for the modern toolchain and dependency set used by this build.

JPEG exception diagnosis, 2026-11-16:

- Record the exception code, fault module/offset, access target, x86 registers
  and stack addresses around the existing JPEG convert call. Continue exception
  search unchanged; do not hide faults or substitute a different encoder.
- Log conversion arguments to distinguish caller input from encoder failures.

Artwork call tracing, 2026-11-16:

- Add BEGIN/END markers around existing provider enumeration, interface lookup,
  IsMine/GetAlbumArtData calls, copying/freeing buffers, the existing fallback
  album-art API and the existing JPEG writer. Keep the current artwork path.
- Record EnableArtwork at initialization so logs identify whether it was tested.
- Read the JPEG writer's type name once and log capability/type checks separately.

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
