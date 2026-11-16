# Remote Speakers Output 6.0

Stream audio to AirPlay/RAOP-compatible speakers and receivers. The current build
provides a 32-bit Winamp output plugin, based on Eric Milles’
[Remote Speakers Output 5.6](http://emilles.dyndns.org/software/out_apx.html).

## Installation

1. Close Winamp and back up any existing plugin DLLs.
2. Extract the archive into the Winamp installation folder, preserving its
   directory structure:
   - `apxCore.dll` goes next to `winamp.exe`.
   - `out_apx.dll` goes into the `Plugins` folder.
3. Start Winamp and open **Preferences → Plug-ins → Output**.
4. Select **Remote Speakers output (6.0)**, click **Configure**, and select
   your receiver.

Install Apple’s Bonjour service for automatic device discovery, or add the
receiver manually in the plugin settings.

## Playback settings

Set **Playback delay (ms)** to a value between **100 and 2000**, click **Apply**,
then press **Stop → Play**. The setting is saved automatically. Lower values
reduce the requested buffering delay; raise it if audio drops out or the start
of a track is missing. The receiver may add its own delay.

The plugin supports track metadata, album artwork, and receiver volume control.
Album artwork is retrieved through Winamp and encoded to JPEG using Windows
Imaging Component (WIC).

## Build locally

Install Visual Studio 2022 C++ desktop tools, including ATL and a Windows SDK,
CMake 3.24 or newer, PowerShell 7, Perl, and tar. From an **x86** Visual Studio
developer environment, run:

```powershell
pwsh -File scripts/build.ps1
```

The DLLs and installation files are written to `dist/`. Dependencies are
version-pinned, verified with SHA-256, and built under `.deps/`.

The automated checks cover DLL loading, startup of the audio engine, metadata
handling, JPEG encoding, and playback-delay settings.

## Source and licensing

The original source is distributed under **GPL-2.0-or-later**. This build combines
it with Apache-2.0 dependencies and is distributed under **GPL-3.0-or-later**,
using the upstream option to choose a later GPL version. See
[LICENSE-GPL-3.0.txt](LICENSE-GPL-3.0.txt) and the dependency notices included in
the binary archive.

When redistributing the DLLs, provide the matching source and dependency sources.
The source package includes the project, build scripts, dependency manifest,
and dependency source archives.

See [MODIFICATIONS.md](MODIFICATIONS.md) for changes from upstream, and
`!README.txt`, `!BUILDING.txt`, and `!HISTORY.txt` for the original documentation.
The original archive is `out_apx-src-5.6.zip`, SHA-256:

```text
a4b5315e8922065a17914ab20c05c1a8d9998f72ebac1449d2a79389682429c3
```
