# Copyright (c) 2018-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

list(APPEND COMPAT_SOURCES
	../openbsd-compat/bsd-getpagesize.c
	../openbsd-compat/explicit_bzero.c
	../openbsd-compat/freezero.c
	../openbsd-compat/recallocarray.c
	../openbsd-compat/strlcat.c
	../openbsd-compat/strlcpy.c
	../openbsd-compat/strsep.c
)

if(WIN32 AND NOT CYGWIN AND NOT MSYS)
	list(APPEND COMPAT_SOURCES
		../openbsd-compat/bsd-getline.c
		../openbsd-compat/endian_win32.c
		../openbsd-compat/explicit_bzero_win32.c
		../openbsd-compat/getopt_long.c
		../openbsd-compat/readpassphrase_win32.c
	)
	if (BUILD_SHARED_LIBS)
		list(APPEND COMPAT_SOURCES ../openbsd-compat/posix_win.c)
	endif()
else()
	list(APPEND COMPAT_SOURCES ../openbsd-compat/readpassphrase.c)
endif()

if(NOT MSVC)
	set_source_files_properties(assert_get.c assert_verify.c base64.c bio.c
	    config.c cred_make.c cred_verify.c credman.c fido2-assert.c
	    fido2-cred.c fido2-token.c pin.c token.c util.c
	    PROPERTIES COMPILE_FLAGS "${EXTRA_CFLAGS}")
endif()

add_executable(fido2-cred
	fido2-cred.c
	cred_make.c
	cred_verify.c
	base64.c
	util.c
	${COMPAT_SOURCES}
)

add_executable(fido2-assert
	fido2-assert.c
	assert_get.c
	assert_verify.c
	base64.c
	util.c
	${COMPAT_SOURCES}
)

add_executable(fido2-token
	fido2-token.c
	base64.c
	bio.c
	config.c
	credman.c
	largeblob.c
	pin.c
	token.c
	util.c
	${COMPAT_SOURCES}
)

target_link_libraries(fido2-cred ${CRYPTO_LIBRARIES} ${_FIDO2_LIBRARY})
target_link_libraries(fido2-assert ${CRYPTO_LIBRARIES} ${_FIDO2_LIBRARY})
target_link_libraries(fido2-token ${CRYPTO_LIBRARIES} ${_FIDO2_LIBRARY})

install(TARGETS fido2-cred fido2-assert fido2-token
	DESTINATION ${CMAKE_INSTALL_BINDIR})
