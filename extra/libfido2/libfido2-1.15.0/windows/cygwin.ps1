# Copyright (c) 2021 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

param(
	[string]$GPGPath = "C:\Program Files (x86)\GnuPG\bin\gpg.exe",
	[string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Cygwin coordinates.
$URL = 'https://www.cygwin.com'
$Setup = 'setup-x86_64.exe'
$Mirror = 'https://mirrors.kernel.org/sourceware/cygwin/'
$Packages = 'gcc-core,pkg-config,cmake,make,libcbor-devel,libssl-devel,zlib-devel'

# Work directories.
$Cygwin = "$PSScriptRoot\..\cygwin"
$Root = "${Cygwin}\root"

# Find GPG.
$GPG = $(Get-Command gpg -ErrorAction Ignore | `
    Select-Object -ExpandProperty Source)
if ([string]::IsNullOrEmpty($GPG)) {
	$GPG = $GPGPath
}
if (-Not (Test-Path $GPG)) {
	throw "Unable to find GPG at $GPG"
}

Write-Host "Config: $Config"
Write-Host "GPG: $GPG"

# Create work directories.
New-Item -Type Directory "${Cygwin}" -Force
New-Item -Type Directory "${Root}" -Force

# Create GNUPGHOME with an empty common.conf to disable use-keyboxd.
# Recent default is to enable keyboxd which in turn ignores --keyring
# arguments.
$GpgHome = "${Cygwin}\.gnupg"
New-Item -Type Directory "${GpgHome}" -Force
New-Item -Type File "${GpgHome}\common.conf" -Force

# Fetch and verify Cygwin.
try {
	if (-Not (Test-Path ${Cygwin}\${Setup} -PathType leaf)) {
		Invoke-WebRequest ${URL}/${Setup} `
		    -OutFile ${Cygwin}\${Setup}
	}
	if (-Not (Test-Path ${Cygwin}\${Setup}.sig -PathType leaf)) {
		Invoke-WebRequest ${URL}/${Setup}.sig `
		    -OutFile ${Cygwin}\${Setup}.sig
	}
	& $GPG --homedir ${GpgHome} --list-keys
	& $GPG --homedir ${GpgHome} --quiet --no-default-keyring `
	    --keyring ${PSScriptRoot}/cygwin.gpg `
	    --verify ${Cygwin}\${Setup}.sig ${Cygwin}\${Setup}
	if ($LastExitCode -ne 0) {
		throw "GPG signature verification failed"
	}
} catch {
	throw "Failed to fetch and verify Cygwin"
}

# Bootstrap Cygwin.
Start-Process "${Cygwin}\${Setup}" -Wait -NoNewWindow `
    -ArgumentList "-dnNOqW -s ${Mirror} -R ${Root} -P ${Packages}"

# Build libfido2.
$Env:PATH = "${Root}\bin\;" + $Env:PATH
cmake "-DCMAKE_BUILD_TYPE=${Config}" -B "build-${Config}"
make -C "build-${Config}"
make -C "build-${Config}" regress
