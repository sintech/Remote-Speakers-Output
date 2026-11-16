# Added 2026-11-16. SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$deps = Join-Path $root '.deps'
$prefix = Join-Path $deps 'install'
New-Item -ItemType Directory -Force "$deps/downloads", $prefix | Out-Null

function Invoke-Checked([string] $program, [string[]] $arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed with exit code $LASTEXITCODE" }
}

# Run from an x86 VS developer prompt. The workflow initializes it automatically.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Run in an x86 Visual Studio developer shell (or use GitHub Actions).'
}
foreach ($dep in (Get-Content "$PSScriptRoot/dependencies.json" -Raw | ConvertFrom-Json)) {
    $filename = if ($dep.filename) { $dep.filename } else { "$($dep.name).tar.gz" }
    $download = Join-Path "$deps/downloads" $filename
    if (-not (Test-Path $download)) {
        Write-Host "Downloading $($dep.name)"
        Invoke-WebRequest -Uri $dep.url -OutFile $download
    }
    if ((Get-FileHash $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dep.sha256) {
        throw "Checksum mismatch: $download"
    }
    $destination = Join-Path $deps $dep.name
    if (-not (Test-Path $destination)) {
        New-Item -ItemType Directory $destination | Out-Null
        if ($dep.filename) { Copy-Item $download $destination }
        else { Invoke-Checked tar @('-xzf', $download, '-C', $destination, '--strip-components=1') }
    }
}

# The upstream RAOPEngine includes decoder and encoder C files into one C++
# translation unit. Apply the upstream BUILDING.txt workaround for duplicate
# helper functions. Keep the downloaded archive unmodified for source delivery.
$encoder = "$deps/alac/codec/ag_enc.c"
$encoderText = Get-Content $encoder -Raw
$encoderText = [regex]::Replace($encoderText,
    '(?s)/\*\s*And on the subject of.*?(?=#if PRAGMA_MARK)',
    "// Patched 2026-11-16 per upstream BUILDING.txt.`n#define abs_func abs`n`n")
[System.IO.File]::WriteAllText($encoder, $encoderText)

if (-not (Test-Path "$prefix/lib/libcrypto.lib")) {
    Push-Location "$deps/openssl"
    try {
        Invoke-Checked perl @('Configure', 'VC-WIN32', 'no-shared', 'no-module', 'no-asm',
            'no-tests', 'no-apps', 'no-docs', '-MT', "--prefix=$prefix", '--libdir=lib')
        Invoke-Checked nmake @('/NOLOGO', 'build_libs')
        Invoke-Checked nmake @('/NOLOGO', 'install_dev')
    } finally { Pop-Location }
}

if (-not (Test-Path "$prefix/lib/cmake/Poco/PocoConfig.cmake")) {
    $pocoArgs = @('-S', "$deps/poco", '-B', "$deps/poco-build", '-G',
        'Visual Studio 17 2022', '-A', 'Win32', "-DCMAKE_INSTALL_PREFIX=$prefix",
        '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DPOCO_STATIC=ON', '-DPOCO_MT=ON',
        '-DENABLE_MSVC_MP=ON', '-DENABLE_NET=ON', '-DENABLE_TESTS=OFF')
    foreach ($component in @('ENCODINGS', 'ENCODINGS_COMPILER', 'XML', 'JSON', 'MONGODB',
        'REDIS', 'PDF', 'UTIL', 'NETSSL', 'NETSSL_WIN', 'CRYPTO', 'DATA', 'DATA_SQLITE',
        'DATA_MYSQL', 'DATA_ODBC', 'SEVENZIP', 'ZIP', 'APACHECONNECTOR', 'CPPPARSER',
        'POCODOC', 'PAGECOMPILER', 'PAGECOMPILER_FILE2PAGE')) {
        $pocoArgs += "-DENABLE_${component}=OFF"
    }
    Invoke-Checked cmake $pocoArgs
    Invoke-Checked cmake @('--build', "$deps/poco-build", '--config', 'Release', '--target', 'install', '--parallel', '2')
}

Invoke-Checked cmake @('-S', $root, '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'Win32')
Invoke-Checked cmake @('--build', 'build', '--config', 'Release', '--parallel', '2')
Invoke-Checked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure')
Invoke-Checked cmake @('--install', 'build', '--config', 'Release', '--prefix', 'dist')

New-Item -ItemType Directory -Force 'dist/licenses' | Out-Null
Copy-Item 'LICENSE*', 'README.md', 'MODIFICATIONS.md', 'scripts/dependencies.json' 'dist/'
Copy-Item "$deps/openssl/LICENSE.txt" 'dist/licenses/OpenSSL.txt'
Copy-Item "$deps/poco/LICENSE" 'dist/licenses/Poco.txt'
Copy-Item "$deps/samplerate/COPYING" 'dist/licenses/libsamplerate.txt'
Copy-Item "$deps/alac/codec/APPLE_LICENSE.txt" 'dist/licenses/ALAC.txt'
Copy-Item "$deps/bonjour/dns_sd.h" 'dist/licenses/Bonjour-header.h'
Get-ChildItem 'dist/*.dll' | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($_.Name)"
} | Set-Content 'dist/SHA256SUMS.txt'
