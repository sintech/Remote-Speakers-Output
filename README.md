# Remote Speakers Output — Winamp x86 build

Source: Eric Milles' [Remote Speakers Output 5.6](http://emilles.dyndns.org/software/out_apx.html).
Original source archive: `out_apx-src-5.6.zip`, SHA-256
`a4b5315e8922065a17914ab20c05c1a8d9998f72ebac1449d2a79389682429c3`.

## Build on GitHub

Push a commit to `build/github-actions` to trigger the workflow, then open
**Actions → Build Winamp plugin (x86)**. Pull requests also trigger a build.
After the workflow exists on the repository's default branch, GitHub additionally
provides **Run workflow** for manual runs with a branch selection.
Download **Remote-Speakers-Output-x86** from a successful run's Artifacts section.
The **Remote-Speakers-Output-source** artifact includes the matching project source
and the original dependency archives used for that build.

The workflow uses Windows Server 2022, MSVC 2022, C++14, Win32/x86, and the static
release C runtime. All five dependencies are version/commit pinned and SHA-256
verified; no Visual Studio 2008/2010 installation or activation service is needed
by this source build. Old Visual Studio project files are kept for reference.

## Install and check

1. Close Winamp and back up the installed plugin DLLs.
2. Extract the binary artifact into Winamp's installation directory, preserving
   folders: `apxCore.dll` goes next to `winamp.exe`, and `Plugins/out_apx.dll`
   goes into `Plugins`. The core belongs next to the executable so Windows can
   find it without modifying the system DLL search path.
3. Start 32-bit Winamp, open **Preferences → Plug-ins → Output** and select
   **Remote Speakers output**. Configure the receiver and test playback.

Bonjour remains optional at runtime; install Apple's Bonjour service for automatic
discovery, or configure the receiver manually. The build only needs its public
`dns_sd.h` header, not the Bonjour installer or import library.

CI checks that the x86 DLL loads and exports a valid Winamp output interface.
This is not a test of audio playback, discovery, metadata, or remote control on a
physical receiver. Compatibility with those features must be tested in Winamp.
The source archive includes blank device icons, as documented by its author.

## Build locally

Install Visual Studio 2022 C++ desktop tools (including ATL), a Windows SDK,
CMake 3.24+ (CMake 4 is supported using the dependency policy setting), PowerShell 7,
Perl, and tar. From an **x86** Visual Studio developer environment run:

```powershell
pwsh -File scripts/build.ps1
```

Outputs are in `dist/`. Dependencies are downloaded and built under `.deps/`.
To rebuild dependencies from scratch, remove `.deps/`. No credentials or secrets
are required by the workflow.

## Reduced latency variant

The **Remote-Speakers-Output-low-latency-x86** artifact sets the RTP sync delay
to **750 ms**, compared with the original **1750 ms**. This requests audio about
one second earlier; the receiver may apply its own delay or minimum latency.
It is experimental: test for missing audio at the start and dropouts on your
receiver. The normal **Remote-Speakers-Output-x86** artifact retains 1750 ms.
Both variants poll connection readiness at 25 ms instead of 250 ms.

For the reduced variant, build locally with:

```powershell
pwsh -File scripts/build.ps1 -SyncLatencyMs 750 -OutputDirectory dist-low-latency
```

Build parameters are recorded in the artifact's `BUILD-OPTIONS.txt`. The maximum
queue latency reported to Winamp is not a measured delay at the speaker.

## Playback diagnostics

Album artwork retrieval from Winamp is **disabled by default** after a startup
crash was traced to this path on a real host. Audio and text metadata remain
enabled. Advanced testing can opt in with `EnableArtwork=1` in the
`[Remote Speakers output]` section of Winamp.ini while Winamp is closed.
The option is preserved when the plugin saves its settings. Keep the default
until artwork has been verified on the target Winamp version.

This build writes a bounded (2 MiB) UTF-8 log to `%TEMP%\out_apx.log`.
Close Winamp, delete an old log if present, start Winamp and try playback once.
The log records initialization stages, receiver connection, RTSP method/status
and exceptions. It does not log raw RTSP authentication headers or audio keys.
Attach the log when reporting a failure. It stays local until you share it.

## Source and licensing

The upstream source headers grant **GPL-2.0-or-later**; `!LICENSE.txt` and the
original GPL v2 `LICENSE` are retained. This build combines the plugin with
Apache-2.0 ALAC/OpenSSL code and is distributed under **GPL-3.0-or-later**, using
the upstream option to choose a later GPL version. See `LICENSE-GPL-3.0.txt` and
the dependency notices packaged under `licenses/`.

Preserve the matching source artifact when redistributing these DLLs. It includes
the build scripts, dependency manifest and dependency source archives.
See `MODIFICATIONS.md` for changes from the upstream archive, and `!README.txt`,
`!BUILDING.txt`, `!HISTORY.txt` for the original documentation. Trial/activation
instructions in the original README describe the author's binary distribution.
