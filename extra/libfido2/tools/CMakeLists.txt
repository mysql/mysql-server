# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

list(APPEND COMPAT_SOURCES
	../openbsd-compat/explicit_bzero.c
	../openbsd-compat/strlcpy.c
	../openbsd-compat/strlcat.c
)

if(WIN32 AND NOT CYGWIN AND NOT MSYS)
	list(APPEND COMPAT_SOURCES
		../openbsd-compat/bsd-getline.c
		../openbsd-compat/explicit_bzero_win32.c
		../openbsd-compat/getopt_long.c
		../openbsd-compat/posix_win.c
		../openbsd-compat/readpassphrase_win32.c
	)
else()
	list(APPEND COMPAT_SOURCES ../openbsd-compat/readpassphrase.c)
endif()

if(NOT MSVC)
	set_source_files_properties(assert_get.c assert_verify.c base64.c bio.c
	    cred_make.c cred_verify.c credman.c fido2-assert.c fido2-cred.c
	    fido2-token.c pin.c token.c util.c PROPERTIES COMPILE_FLAGS
            "-Wconversion -Wsign-conversion")
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
	credman.c
	pin.c
	token.c
	util.c
	${COMPAT_SOURCES}
)

target_link_libraries(fido2-cred ${CRYPTO_LIBRARY} fido2_shared)
target_link_libraries(fido2-assert ${CRYPTO_LIBRARY} fido2_shared)
target_link_libraries(fido2-token ${CRYPTO_LIBRARY} fido2_shared)

install(TARGETS fido2-cred fido2-assert fido2-token
	DESTINATION ${CMAKE_INSTALL_BINDIR})
