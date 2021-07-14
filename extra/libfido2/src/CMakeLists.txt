# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

add_definitions(-D_FIDO_INTERNAL)

list(APPEND FIDO_SOURCES
	aes256.c
	assert.c
	authkey.c
	bio.c
	blob.c
	buf.c
	cbor.c
	cred.c
	credman.c
	dev.c
	ecdh.c
	eddsa.c
	err.c
	es256.c
	hid.c
	info.c
	io.c
	iso7816.c
	log.c
	pin.c
	reset.c
	rs256.c
	u2f.c
)

if(FUZZ)
	list(APPEND FIDO_SOURCES ../fuzz/prng.c)
	list(APPEND FIDO_SOURCES ../fuzz/uniform_random.c)
	list(APPEND FIDO_SOURCES ../fuzz/wrap.c)
endif()

if(USE_HIDAPI)
	list(APPEND FIDO_SOURCES hid_hidapi.c)
elseif(WIN32)
	list(APPEND FIDO_SOURCES hid_win.c)
elseif(APPLE)
	list(APPEND FIDO_SOURCES hid_osx.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND FIDO_SOURCES hid_linux.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	list(APPEND FIDO_SOURCES hid_openbsd.c)
else()
	message(FATAL_ERROR "please define a hid backend for your platform")
endif()

if(NOT MSVC)
	set_source_files_properties(${FIDO_SOURCES} PROPERTIES COMPILE_FLAGS
	    "-Wconversion -Wsign-conversion")
endif()

list(APPEND COMPAT_SOURCES
	../openbsd-compat/bsd-getpagesize.c
	../openbsd-compat/explicit_bzero.c
	../openbsd-compat/explicit_bzero_win32.c
	../openbsd-compat/recallocarray.c
	../openbsd-compat/strlcat.c
	../openbsd-compat/timingsafe_bcmp.c
)

# dynamic library
add_library(fido2 SHARED ${FIDO_SOURCES} ${COMPAT_SOURCES})

target_link_libraries(fido2 cbor ${CRYPTO_LIBRARY}
	${UDEV_LIBRARIES} ${BASE_LIBRARIES} ${HIDAPI_LIBRARIES})
if(WIN32)
	if (MINGW)
		target_link_libraries(fido2 wsock32 ws2_32 bcrypt
			setupapi hid)
	else()
		target_link_libraries(fido2 wsock32 ws2_32 bcrypt
			SetupAPI hid)
	endif()
elseif(APPLE)
	target_link_libraries(fido2 "-framework CoreFoundation"
		"-framework IOKit")
endif()

set_target_properties(fido2 PROPERTIES OUTPUT_NAME fido2
	VERSION ${FIDO_VERSION} SOVERSION ${FIDO_MAJOR})

SET_TARGET_PROPERTIES(fido2 PROPERTIES
  DEBUG_POSTFIX ""
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  )

IF(WITH_AUTHENTICATION_FIDO OR WITH_AUTHENTICATION_CLIENT_PLUGINS)
  INSTALL_PRIVATE_LIBRARY(fido2)
ENDIF()
ADD_OBJDUMP_TARGET(show_fido2 "$<TARGET_FILE:fido2>" DEPENDS fido2)

# On Linux libcrypto.so.1.1 will be installed in the same
# directory as libfido2.so.1
IF(CRYPTO_CUSTOM_LIBRARY)
  ADD_INSTALL_RPATH(fido2 "\$ORIGIN")
ENDIF()

SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(fido2)
