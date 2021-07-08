param(
	[string]$CMakePath = "C:\Program Files\CMake\bin\cmake.exe",
	[string]$GitPath = "C:\Program Files\Git\bin\git.exe",
	[string]$SevenZPath = "C:\Program Files\7-Zip\7z.exe",
	[string]$GPGPath = "C:\Program Files (x86)\GnuPG\bin\gpg.exe"
)

$ErrorActionPreference = "Continue"

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# LibreSSL coordinates.
New-Variable -Name 'LIBRESSL_URL' `
	-Value 'https://ftp.openbsd.org/pub/OpenBSD/LibreSSL' -Option Constant
New-Variable -Name 'LIBRESSL' -Value 'libressl-3.1.4' -Option Constant

# libcbor coordinates.
New-Variable -Name 'LIBCBOR' -Value 'libcbor-0.7.0' -Option Constant
New-Variable -Name 'LIBCBOR_BRANCH' -Value 'v0.7.0' -Option Constant
New-Variable -Name 'LIBCBOR_GIT' -Value 'https://github.com/pjk/libcbor' `
	-Option Constant

# Work directories.
New-Variable -Name 'BUILD' -Value "$PSScriptRoot\..\build" -Option Constant
New-Variable -Name 'OUTPUT' -Value "$PSScriptRoot\..\output" -Option Constant

# Find CMake.
$CMake = $(Get-Command cmake -ErrorAction Ignore | Select-Object -ExpandProperty Source)
if([string]::IsNullOrEmpty($CMake)) {
	$CMake = $CMakePath
}

# Find Git.
$Git = $(Get-Command git -ErrorAction Ignore | Select-Object -ExpandProperty Source)
if([string]::IsNullOrEmpty($Git)) {
	$Git = $GitPath
}

# Find 7z.
$SevenZ = $(Get-Command 7z -ErrorAction Ignore | Select-Object -ExpandProperty Source)
if([string]::IsNullOrEmpty($SevenZ)) {
	$SevenZ = $SevenZPath
}

# Find GPG.
$GPG = $(Get-Command gpg -ErrorAction Ignore | Select-Object -ExpandProperty Source)
if([string]::IsNullOrEmpty($GPG)) {
	$GPG = $GPGPath
}

if(-Not (Test-Path $CMake)) {
	throw "Unable to find CMake at $CMake"
}

if(-Not (Test-Path $Git)) {
	throw "Unable to find Git at $Git"
}

if(-Not (Test-Path $SevenZ)) {
	throw "Unable to find 7z at $SevenZ"
}

if(-Not (Test-Path $GPG)) {
	throw "Unable to find GPG at $GPG"
}

Write-Host "Git: $Git"
Write-Host "CMake: $CMake"
Write-Host "7z: $SevenZ"
Write-Host "GPG: $GPG"

New-Item -Type Directory ${BUILD}
New-Item -Type Directory ${BUILD}\32
New-Item -Type Directory ${BUILD}\64
New-Item -Type Directory ${OUTPUT}
New-Item -Type Directory ${OUTPUT}\pkg\Win64\Release\v142\dynamic
New-Item -Type Directory ${OUTPUT}\pkg\Win32\Release\v142\dynamic

Push-Location ${BUILD}

try {
	if (Test-Path .\${LIBRESSL}) {
		Remove-Item .\${LIBRESSL} -Recurse -ErrorAction Stop
	}

	if(-Not (Test-Path .\${LIBRESSL}.tar.gz -PathType leaf)) {
		Invoke-WebRequest ${LIBRESSL_URL}/${LIBRESSL}.tar.gz `
			-OutFile .\${LIBRESSL}.tar.gz
	}
	if(-Not (Test-Path .\${LIBRESSL}.tar.gz.asc -PathType leaf)) {
		Invoke-WebRequest ${LIBRESSL_URL}/${LIBRESSL}.tar.gz.asc `
			-OutFile .\${LIBRESSL}.tar.gz.asc
	}

	Copy-Item "$PSScriptRoot\libressl.gpg" -Destination "${BUILD}"
	& $GPG --list-keys
	& $GPG -v --no-default-keyring --keyring ./libressl.gpg `
		--verify .\${LIBRESSL}.tar.gz.asc .\${LIBRESSL}.tar.gz
	if ($LastExitCode -ne 0) {
		throw "GPG signature verification failed"
	}

	& $SevenZ e .\${LIBRESSL}.tar.gz
	& $SevenZ x .\${LIBRESSL}.tar
	Remove-Item -Force .\${LIBRESSL}.tar

	if(-Not (Test-Path .\${LIBCBOR})) {
		Write-Host "Cloning ${LIBCBOR}..."
		& $Git clone --branch ${LIBCBOR_BRANCH} ${LIBCBOR_GIT} `
			.\${LIBCBOR}
	}
} catch {
	throw "Failed to fetch and verify dependencies"
} finally {
	Pop-Location
}

Function Build(${OUTPUT}, ${GENERATOR}, ${ARCH}) {
	if(-Not (Test-Path .\${LIBRESSL})) {
		New-Item -Type Directory .\${LIBRESSL} -ErrorAction Stop
	}

	Push-Location .\${LIBRESSL}
	& $CMake ..\..\${LIBRESSL} -G "${GENERATOR}" -A "${ARCH}" `
		-DCMAKE_C_FLAGS_RELEASE="/Zi" `
		-DCMAKE_INSTALL_PREFIX="${OUTPUT}" -DBUILD_SHARED_LIBS=ON `
		-DLIBRESSL_TESTS=OFF
	& $CMake --build . --config Release
	& $CMake --build . --config Release --target install
	Pop-Location

	if(-Not (Test-Path .\${LIBCBOR})) {
		New-Item -Type Directory .\${LIBCBOR} -ErrorAction Stop
	}

	Push-Location .\${LIBCBOR}
	& $CMake ..\..\${LIBCBOR} -G "${GENERATOR}" -A "${ARCH}" `
		-DCMAKE_C_FLAGS_RELEASE="/Zi" `
		-DCMAKE_INSTALL_PREFIX="${OUTPUT}"
	& $CMake --build . --config Release
	& $CMake --build . --config Release --target install
	Pop-Location

	& $CMake ..\.. -G "${GENERATOR}" -A "${ARCH}" `
		-DCBOR_INCLUDE_DIRS="${OUTPUT}\include" `
		-DCBOR_LIBRARY_DIRS="${OUTPUT}\lib" `
		-DCRYPTO_INCLUDE_DIRS="${OUTPUT}\include" `
		-DCRYPTO_LIBRARY_DIRS="${OUTPUT}\lib" `
		-DCMAKE_INSTALL_PREFIX="${OUTPUT}"
	& $CMake --build . --config Release
	& $CMake --build . --config Release --target install
	"cbor.dll", "crypto-46.dll" | %{ Copy-Item "${OUTPUT}\bin\$_" `
		-Destination "examples\Release" }
}

Function Package-Headers() {
	Copy-Item "${OUTPUT}\64\include" -Destination "${OUTPUT}\pkg" `
		-Recurse -ErrorAction Stop
}

Function Package-Libraries(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\bin\cbor.dll" "${DEST}" -ErrorAction Stop
	Copy-Item "${SRC}\lib\cbor.lib" "${DEST}" -ErrorAction Stop
	Copy-Item "${SRC}\bin\crypto-46.dll" "${DEST}" -ErrorAction Stop
	Copy-Item "${SRC}\lib\crypto-46.lib" "${DEST}" -ErrorAction Stop
	Copy-Item "${SRC}\lib\fido2.dll" "${DEST}" -ErrorAction Stop
	Copy-Item "${SRC}\lib\fido2.lib" "${DEST}" -ErrorAction Stop
}

Function Package-PDBs(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\${LIBRESSL}\crypto\crypto.dir\Release\vc142.pdb" `
		"${DEST}\crypto-46.pdb" -ErrorAction Stop
	Copy-Item "${SRC}\${LIBCBOR}\src\cbor_shared.dir\Release\vc142.pdb" `
		"${DEST}\cbor.pdb" -ErrorAction Stop
	Copy-Item "${SRC}\src\fido2_shared.dir\Release\vc142.pdb" `
		"${DEST}\fido2.pdb" -ErrorAction Stop
}

Function Package-Tools(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\tools\Release\fido2-assert.exe" `
		"${DEST}\fido2-assert.exe" -ErrorAction stop
	Copy-Item "${SRC}\tools\Release\fido2-cred.exe" `
		"${DEST}\fido2-cred.exe" -ErrorAction stop
	Copy-Item "${SRC}\tools\Release\fido2-token.exe" `
		"${DEST}\fido2-token.exe" -ErrorAction stop
}

Push-Location ${BUILD}\64
Build ${OUTPUT}\64 "Visual Studio 16 2019" "x64"
Pop-Location

Push-Location ${BUILD}\32
Build ${OUTPUT}\32 "Visual Studio 16 2019" "Win32"
Pop-Location

Package-Headers

Package-Libraries ${OUTPUT}\64 ${OUTPUT}\pkg\Win64\Release\v142\dynamic
Package-PDBs ${BUILD}\64 ${OUTPUT}\pkg\Win64\Release\v142\dynamic
Package-Tools ${BUILD}\64 ${OUTPUT}\pkg\Win64\Release\v142\dynamic

Package-Libraries ${OUTPUT}\32 ${OUTPUT}\pkg\Win32\Release\v142\dynamic
Package-PDBs ${BUILD}\32 ${OUTPUT}\pkg\Win32\Release\v142\dynamic
Package-Tools ${BUILD}\32 ${OUTPUT}\pkg\Win32\Release\v142\dynamic
