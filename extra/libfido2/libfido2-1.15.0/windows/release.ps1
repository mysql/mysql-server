# Copyright (c) 2021-2024 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

$ErrorActionPreference = "Stop"
$Architectures = @('x64', 'Win32', 'ARM64', 'ARM')
$InstallPrefixes =  @('Win64', 'Win32', 'ARM64', 'ARM')
$Types = @('dynamic', 'static')
$Config = 'Release'
$SDK = '143'

. "$PSScriptRoot\const.ps1"

foreach ($Arch in $Architectures) {
	foreach ($Type in $Types) {
		./build.ps1 -Arch ${Arch} -Type ${Type} -Config ${Config}
	}
}

foreach ($InstallPrefix in $InstallPrefixes) {
	foreach ($Type in $Types) {
		New-Item -Type Directory `
		    "${OUTPUT}/pkg/${InstallPrefix}/${Config}/v${SDK}/${Type}"
	}
}

Function Package-Headers() {
	Copy-Item "${OUTPUT}\x64\dynamic\include" -Destination "${OUTPUT}\pkg" `
	    -Recurse -ErrorAction Stop
}

Function Package-Dynamic(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\bin\cbor.dll" "${DEST}"
	Copy-Item "${SRC}\lib\cbor.lib" "${DEST}"
	Copy-Item "${SRC}\bin\zlib1.dll" "${DEST}"
	Copy-Item "${SRC}\lib\zlib1.lib" "${DEST}"
	Copy-Item "${SRC}\bin\${CRYPTO_LIBRARIES}.dll" "${DEST}"
	Copy-Item "${SRC}\lib\${CRYPTO_LIBRARIES}.lib" "${DEST}"
	Copy-Item "${SRC}\bin\fido2.dll" "${DEST}"
	Copy-Item "${SRC}\lib\fido2.lib" "${DEST}"
}

Function Package-Static(${SRC}, ${DEST}) {
	Copy-Item "${SRC}/lib/cbor.lib" "${DEST}"
	Copy-Item "${SRC}/lib/zlib1.lib" "${DEST}"
	Copy-Item "${SRC}/lib/${CRYPTO_LIBRARIES}.lib" "${DEST}"
	Copy-Item "${SRC}/lib/fido2_static.lib" "${DEST}/fido2.lib"
}

Function Package-PDBs(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\${LIBRESSL}\crypto\crypto_obj.dir\${Config}\crypto_obj.pdb" `
	    "${DEST}\${CRYPTO_LIBRARIES}.pdb"
	Copy-Item "${SRC}\${LIBCBOR}\src\cbor.dir\${Config}\vc${SDK}.pdb" `
	    "${DEST}\cbor.pdb"
	Copy-Item "${SRC}\${ZLIB}\zlib.dir\${Config}\vc${SDK}.pdb" `
	    "${DEST}\zlib1.pdb"
	Copy-Item "${SRC}\src\fido2_shared.dir\${Config}\vc${SDK}.pdb" `
	    "${DEST}\fido2.pdb"
}

Function Package-StaticPDBs(${SRC}, ${DEST}) {
	# NOTE: original file names must be preserved
	Copy-Item "${SRC}\${LIBRESSL}\crypto\crypto_obj.dir\${Config}\crypto_obj.pdb" `
	    "${DEST}"
	Copy-Item "${SRC}\${LIBCBOR}\src\${Config}\cbor.pdb" "${DEST}"
	Copy-Item "${SRC}\${ZLIB}\${Config}\zlibstatic.pdb" "${DEST}"
	Copy-Item "${SRC}\src\${Config}\fido2_static.pdb" "${DEST}"
}

Function Package-Tools(${SRC}, ${DEST}) {
	Copy-Item "${SRC}\tools\${Config}\fido2-assert.exe" `
	    "${DEST}\fido2-assert.exe"
	Copy-Item "${SRC}\tools\${Config}\fido2-cred.exe" `
	    "${DEST}\fido2-cred.exe"
	Copy-Item "${SRC}\tools\${Config}\fido2-token.exe" `
	    "${DEST}\fido2-token.exe"
}

Package-Headers

for ($i = 0; $i -lt $Architectures.Length; $i++) {
	$Arch = $Architectures[$i]
	$InstallPrefix = $InstallPrefixes[$i]
	Package-Dynamic "${OUTPUT}\${Arch}\dynamic" `
	    "${OUTPUT}\pkg\${InstallPrefix}\${Config}\v${SDK}\dynamic"
	Package-PDBs "${BUILD}\${Arch}\dynamic" `
	    "${OUTPUT}\pkg\${InstallPrefix}\${Config}\v${SDK}\dynamic"
	Package-Tools "${BUILD}\${Arch}\dynamic" `
	    "${OUTPUT}\pkg\${InstallPrefix}\${Config}\v${SDK}\dynamic"
	Package-Static "${OUTPUT}\${Arch}\static" `
	    "${OUTPUT}\pkg\${InstallPrefix}\${Config}\v${SDK}\static"
	Package-StaticPDBs "${BUILD}\${Arch}\static" `
	    "${OUTPUT}\pkg\${InstallPrefix}\${Config}\v${SDK}\static"
}
