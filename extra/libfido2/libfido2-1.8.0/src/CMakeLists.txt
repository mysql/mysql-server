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
	compress.c
	config.c
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
	largeblob.c
	log.c
	pin.c
	random.c
	reset.c
	rs256.c
	u2f.c
)

if(FUZZ)
	list(APPEND FIDO_SOURCES ../fuzz/prng.c)
	list(APPEND FIDO_SOURCES ../fuzz/uniform_random.c)
	list(APPEND FIDO_SOURCES ../fuzz/udev.c)
	list(APPEND FIDO_SOURCES ../fuzz/wrap.c)
endif()
if(NFC_LINUX)
	list(APPEND FIDO_SOURCES netlink.c nfc_linux.c)
endif()

if(USE_HIDAPI)
	list(APPEND FIDO_SOURCES hid_hidapi.c)
	if(NOT WIN32 AND NOT APPLE)
		list(APPEND FIDO_SOURCES hid_unix.c)
	endif()
elseif(WIN32)
	list(APPEND FIDO_SOURCES hid_win.c)
	if(USE_WINHELLO)
		list(APPEND FIDO_SOURCES winhello.c)
	endif()
elseif(APPLE)
	list(APPEND FIDO_SOURCES hid_osx.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND FIDO_SOURCES hid_linux.c hid_unix.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	list(APPEND FIDO_SOURCES hid_netbsd.c hid_unix.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	list(APPEND FIDO_SOURCES hid_openbsd.c hid_unix.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	list(APPEND FIDO_SOURCES hid_freebsd.c hid_unix.c)
else()
	message(FATAL_ERROR "please define a hid backend for your platform")
endif()

if(NOT MSVC)
	set_source_files_properties(${FIDO_SOURCES} PROPERTIES COMPILE_FLAGS
	    "-Wconversion -Wsign-conversion")
endif()

list(APPEND COMPAT_SOURCES
	../openbsd-compat/bsd-getpagesize.c
	../openbsd-compat/endian_win32.c
	../openbsd-compat/explicit_bzero.c
	../openbsd-compat/explicit_bzero_win32.c
	../openbsd-compat/freezero.c
	../openbsd-compat/hkdf.c
	../openbsd-compat/recallocarray.c
	../openbsd-compat/strlcat.c
	../openbsd-compat/timingsafe_bcmp.c
)

if(WIN32)
	list(APPEND BASE_LIBRARIES wsock32 ws2_32 bcrypt setupapi hid)
	if(USE_WINHELLO)
		list(APPEND BASE_LIBRARIES webauthn)
	endif()
elseif(APPLE)
	list(APPEND BASE_LIBRARIES "-framework CoreFoundation" "-framework IOKit")
endif()

list(APPEND TARGET_LIBRARIES
	${CBOR_LIBRARIES}
	${CRYPTO_LIBRARIES}
	${UDEV_LIBRARIES}
	${BASE_LIBRARIES}
	${HIDAPI_LIBRARIES}
	${ZLIB_LIBRARIES}
)

# static library
if(BUILD_STATIC_LIBS)
	add_library(fido2 STATIC ${FIDO_SOURCES} ${COMPAT_SOURCES})
	if(WIN32 AND NOT MINGW)
		set_target_properties(fido2 PROPERTIES OUTPUT_NAME fido2_static)
	endif()
	target_link_libraries(fido2 ${TARGET_LIBRARIES})
	install(TARGETS fido2 ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

# dynamic library
if(BUILD_SHARED_LIBS)
	add_library(fido2_shared SHARED ${FIDO_SOURCES} ${COMPAT_SOURCES})
	set_target_properties(fido2_shared PROPERTIES OUTPUT_NAME fido2
		VERSION ${FIDO_VERSION} SOVERSION ${FIDO_MAJOR})
	target_link_libraries(fido2_shared ${TARGET_LIBRARIES})
	install(TARGETS fido2_shared
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES fido.h DESTINATION include)
install(DIRECTORY fido DESTINATION include)

if(NOT WIN32)
	configure_file(libfido2.pc.in libfido2.pc @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libfido2.pc"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
