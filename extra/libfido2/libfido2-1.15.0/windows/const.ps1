# Copyright (c) 2021-2024 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

# LibreSSL coordinates.
New-Variable -Name 'LIBRESSL_URL' `
    -Value 'https://ftp.openbsd.org/pub/OpenBSD/LibreSSL' `
    -Option Constant
New-Variable -Name 'LIBRESSL' -Value 'libressl-3.9.2' -Option Constant
New-Variable -Name 'CRYPTO_LIBRARIES' -Value 'crypto' -Option Constant

# libcbor coordinates.
New-Variable -Name 'LIBCBOR' -Value 'libcbor-0.11.0' -Option Constant
New-Variable -Name 'LIBCBOR_BRANCH' -Value 'v0.11.0' -Option Constant
New-Variable -Name 'LIBCBOR_GIT' -Value 'https://github.com/pjk/libcbor' `
    -Option Constant

# zlib coordinates.
New-Variable -Name 'ZLIB' -Value 'zlib-1.3.1' -Option Constant
New-Variable -Name 'ZLIB_BRANCH' -Value 'v1.3.1' -Option Constant
New-Variable -Name 'ZLIB_GIT' -Value 'https://github.com/madler/zlib' `
    -Option Constant

# Work directories.
New-Variable -Name 'BUILD' -Value "$PSScriptRoot\..\build" -Option Constant
New-Variable -Name 'OUTPUT' -Value "$PSScriptRoot\..\output" -Option Constant

# Prefixes.
New-Variable -Name 'STAGE' -Value "${BUILD}\${Arch}\${Type}" -Option Constant
New-Variable -Name 'PREFIX' -Value "${OUTPUT}\${Arch}\${Type}" -Option Constant

# Build flags.
if ("${Type}" -eq "dynamic") {
	New-Variable -Name 'RUNTIME' -Value '/MD' -Option Constant
	New-Variable -Name 'SHARED' -Value 'ON' -Option Constant
	New-Variable -Name 'CMAKE_MSVC_RUNTIME_LIBRARY' -Option Constant `
	    -Value 'MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'
} else {
	New-Variable -Name 'RUNTIME' -Value '/MT' -Option Constant
	New-Variable -Name 'SHARED' -Value 'OFF' -Option Constant
	New-Variable -Name 'CMAKE_MSVC_RUNTIME_LIBRARY' -Option Constant `
	    -Value 'MultiThreaded$<$<CONFIG:Debug>:Debug>'
}
New-Variable -Name 'CFLAGS_DEBUG' -Value "${RUNTIME}d /Zi /guard:cf /sdl" `
    -Option Constant
New-Variable -Name 'CFLAGS_RELEASE' -Value "${RUNTIME} /Zi /guard:cf /sdl" `
    -Option Constant
