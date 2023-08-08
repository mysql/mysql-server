# Copyright (c) 2021-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

param(
	[string]$CMakePath = "C:\Program Files\CMake\bin\cmake.exe",
	[string]$GitPath = "C:\Program Files\Git\bin\git.exe",
	[string]$SevenZPath = "C:\Program Files\7-Zip\7z.exe",
	[string]$GPGPath = "C:\Program Files (x86)\GnuPG\bin\gpg.exe",
	[string]$WinSDK = "",
	[string]$Config = "Release",
	[string]$Arch = "x64",
	[string]$Type = "dynamic",
	[string]$Fido2Flags = ""
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

. "$PSScriptRoot\const.ps1"

Function ExitOnError() {
	if ($LastExitCode -ne 0) {
		throw "A command exited with status $LastExitCode"
	}
}

Function GitClone(${REPO}, ${BRANCH}, ${DIR}) {
	Write-Host "Cloning ${REPO}..."
	& $Git -c advice.detachedHead=false clone --quiet --depth=1 `
	    --branch "${BRANCH}" "${REPO}" "${DIR}"
	Write-Host "${REPO}'s ${BRANCH} HEAD is:"
	& $Git -C "${DIR}" show -s HEAD
}

# Find Git.
$Git = $(Get-Command git -ErrorAction Ignore | `
    Select-Object -ExpandProperty Source)
if ([string]::IsNullOrEmpty($Git)) {
	$Git = $GitPath
}
if (-Not (Test-Path $Git)) {
	throw "Unable to find Git at $Git"
}

# Find CMake.
$CMake = $(Get-Command cmake -ErrorAction Ignore | `
    Select-Object -ExpandProperty Source)
if ([string]::IsNullOrEmpty($CMake)) {
	$CMake = $CMakePath
}
if (-Not (Test-Path $CMake)) {
	throw "Unable to find CMake at $CMake"
}

# Find 7z.
$SevenZ = $(Get-Command 7z -ErrorAction Ignore | `
    Select-Object -ExpandProperty Source)
if ([string]::IsNullOrEmpty($SevenZ)) {
	$SevenZ = $SevenZPath
}
if (-Not (Test-Path $SevenZ)) {
	throw "Unable to find 7z at $SevenZ"
}

# Find GPG.
$GPG = $(Get-Command gpg -ErrorAction Ignore | `
    Select-Object -ExpandProperty Source)
if ([string]::IsNullOrEmpty($GPG)) {
	$GPG = $GPGPath
}
if (-Not (Test-Path $GPG)) {
	throw "Unable to find GPG at $GPG"
}

# Override CMAKE_SYSTEM_VERSION if $WinSDK is set.
if (-Not ([string]::IsNullOrEmpty($WinSDK))) {
	$CMAKE_SYSTEM_VERSION = "-DCMAKE_SYSTEM_VERSION='$WinSDK'"
} else {
	$CMAKE_SYSTEM_VERSION = ''
}

Write-Host "WinSDK: $WinSDK"
Write-Host "Config: $Config"
Write-Host "Arch: $Arch"
Write-Host "Type: $Type"
Write-Host "Git: $Git"
Write-Host "CMake: $CMake"
Write-Host "7z: $SevenZ"
Write-Host "GPG: $GPG"

# Create build directories.
New-Item -Type Directory "${BUILD}" -Force
New-Item -Type Directory "${BUILD}\${Arch}" -Force
New-Item -Type Directory "${BUILD}\${Arch}\${Type}" -Force
New-Item -Type Directory "${STAGE}\${LIBRESSL}" -Force
New-Item -Type Directory "${STAGE}\${LIBCBOR}" -Force
New-Item -Type Directory "${STAGE}\${ZLIB}" -Force

# Create output directories.
New-Item -Type Directory "${OUTPUT}" -Force
New-Item -Type Directory "${OUTPUT}\${Arch}" -Force
New-Item -Type Directory "${OUTPUT}\${Arch}\${Type}" -force

# Fetch and verify dependencies.
Push-Location ${BUILD}
try {
	if (-Not (Test-Path .\${LIBRESSL})) {
		if (-Not (Test-Path .\${LIBRESSL}.tar.gz -PathType leaf)) {
			Invoke-WebRequest ${LIBRESSL_URL}/${LIBRESSL}.tar.gz `
			    -OutFile .\${LIBRESSL}.tar.gz
		}
		if (-Not (Test-Path .\${LIBRESSL}.tar.gz.asc -PathType leaf)) {
			Invoke-WebRequest ${LIBRESSL_URL}/${LIBRESSL}.tar.gz.asc `
			    -OutFile .\${LIBRESSL}.tar.gz.asc
		}

		Copy-Item "$PSScriptRoot\libressl.gpg" -Destination "${BUILD}"
		& $GPG --list-keys
		& $GPG --quiet --no-default-keyring --keyring ./libressl.gpg `
		    --verify .\${LIBRESSL}.tar.gz.asc .\${LIBRESSL}.tar.gz
		if ($LastExitCode -ne 0) {
			throw "GPG signature verification failed"
		}
		& $SevenZ e .\${LIBRESSL}.tar.gz
		& $SevenZ x .\${LIBRESSL}.tar
		Remove-Item -Force .\${LIBRESSL}.tar
	}
	if (-Not (Test-Path .\${LIBCBOR})) {
		GitClone "${LIBCBOR_GIT}" "${LIBCBOR_BRANCH}" ".\${LIBCBOR}"
	}
	if (-Not (Test-Path .\${ZLIB})) {
		GitClone "${ZLIB_GIT}" "${ZLIB_BRANCH}" ".\${ZLIB}"
	}
} catch {
	throw "Failed to fetch and verify dependencies"
} finally {
	Pop-Location
}

# Build LibreSSL.
Push-Location ${STAGE}\${LIBRESSL}
try {
	& $CMake ..\..\..\${LIBRESSL} -A "${Arch}" `
	    -DBUILD_SHARED_LIBS="${SHARED}" -DLIBRESSL_TESTS=OFF `
	    -DCMAKE_C_FLAGS_DEBUG="${CFLAGS_DEBUG}" `
	    -DCMAKE_C_FLAGS_RELEASE="${CFLAGS_RELEASE}" `
	    -DCMAKE_INSTALL_PREFIX="${PREFIX}" "${CMAKE_SYSTEM_VERSION}"; `
	    ExitOnError
	& $CMake --build . --config ${Config} --verbose; ExitOnError
	& $CMake --build . --config ${Config} --target install --verbose; `
	    ExitOnError
} catch {
	throw "Failed to build LibreSSL"
} finally {
	Pop-Location
}

# Build libcbor.
Push-Location ${STAGE}\${LIBCBOR}
try {
	& $CMake ..\..\..\${LIBCBOR} -A "${Arch}" `
	    -DWITH_EXAMPLES=OFF `
	    -DBUILD_SHARED_LIBS="${SHARED}" `
	    -DCMAKE_C_FLAGS_DEBUG="${CFLAGS_DEBUG} /wd4703" `
	    -DCMAKE_C_FLAGS_RELEASE="${CFLAGS_RELEASE} /wd4703" `
	    -DCMAKE_INSTALL_PREFIX="${PREFIX}" "${CMAKE_SYSTEM_VERSION}"; `
	    ExitOnError
	& $CMake --build . --config ${Config} --verbose; ExitOnError
	& $CMake --build . --config ${Config} --target install --verbose; `
	    ExitOnError
} catch {
	throw "Failed to build libcbor"
} finally {
	Pop-Location
}

# Build zlib.
Push-Location ${STAGE}\${ZLIB}
try {
	& $CMake ..\..\..\${ZLIB} -A "${Arch}" `
	    -DBUILD_SHARED_LIBS="${SHARED}" `
	    -DCMAKE_C_FLAGS_DEBUG="${CFLAGS_DEBUG}" `
	    -DCMAKE_C_FLAGS_RELEASE="${CFLAGS_RELEASE}" `
	    -DCMAKE_INSTALL_PREFIX="${PREFIX}" "${CMAKE_SYSTEM_VERSION}"; `
	    ExitOnError
	& $CMake --build . --config ${Config} --verbose; ExitOnError
	& $CMake --build . --config ${Config} --target install --verbose; `
	    ExitOnError
	# Patch up zlib's various names.
	if ("${Type}" -eq "Dynamic") {
		((Get-ChildItem -Path "${PREFIX}/lib") -Match "zlib[d]?.lib") |
		    Copy-Item -Destination "${PREFIX}/lib/zlib1.lib" -Force
		((Get-ChildItem -Path "${PREFIX}/bin") -Match "zlibd1.dll") |
		    Copy-Item -Destination "${PREFIX}/bin/zlib1.dll" -Force
	} else {
		((Get-ChildItem -Path "${PREFIX}/lib") -Match "zlibstatic[d]?.lib") |
		    Copy-item -Destination "${PREFIX}/lib/zlib1.lib" -Force
	}
} catch {
	throw "Failed to build zlib"
} finally {
	Pop-Location
}

# Build libfido2.
Push-Location ${STAGE}
try {
	& $CMake ..\..\.. -A "${Arch}" `
	    -DCMAKE_BUILD_TYPE="${Config}" `
	    -DBUILD_SHARED_LIBS="${SHARED}" `
	    -DCBOR_INCLUDE_DIRS="${PREFIX}\include" `
	    -DCBOR_LIBRARY_DIRS="${PREFIX}\lib" `
	    -DCBOR_BIN_DIRS="${PREFIX}\bin" `
	    -DZLIB_INCLUDE_DIRS="${PREFIX}\include" `
	    -DZLIB_LIBRARY_DIRS="${PREFIX}\lib" `
	    -DZLIB_BIN_DIRS="${PREFIX}\bin" `
	    -DCRYPTO_INCLUDE_DIRS="${PREFIX}\include" `
	    -DCRYPTO_LIBRARY_DIRS="${PREFIX}\lib" `
	    -DCRYPTO_BIN_DIRS="${PREFIX}\bin" `
	    -DCRYPTO_LIBRARIES="${CRYPTO_LIBRARIES}" `
	    -DCMAKE_C_FLAGS_DEBUG="${CFLAGS_DEBUG} ${Fido2Flags}" `
	    -DCMAKE_C_FLAGS_RELEASE="${CFLAGS_RELEASE} ${Fido2Flags}" `
	    -DCMAKE_INSTALL_PREFIX="${PREFIX}" "${CMAKE_SYSTEM_VERSION}"; `
	    ExitOnError
	& $CMake --build . --config ${Config} --verbose; ExitOnError
	& $CMake --build . --config ${Config} --target regress --verbose; `
	    ExitOnError
	& $CMake --build . --config ${Config} --target install --verbose; `
	    ExitOnError
	# Copy DLLs.
	if ("${SHARED}" -eq "ON") {
		"cbor.dll", "${CRYPTO_LIBRARIES}.dll", "zlib1.dll" | `
		    %{ Copy-Item "${PREFIX}\bin\$_" `
		    -Destination "examples\${Config}" }
	}
} catch {
	throw "Failed to build libfido2"
} finally {
	Pop-Location
}
