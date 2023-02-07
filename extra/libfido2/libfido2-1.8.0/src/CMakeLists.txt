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

DOWNGRADE_OPENSSL3_DEPRECATION_WARNINGS(
  assert.c
  cbor.c
  cred.c
  es256.c
  rs256.c
  ecdh.c
  )

IF(OPENSSL_VERSION_MAJOR VERSION_EQUAL 3)
  IF(MY_COMPILER_IS_GNU)
    # Downgrade to warning, for now.
    ADD_COMPILE_FLAGS(
      assert.c
      cbor.c
      cred.c
      es256.c
      rs256.c
      COMPILE_FLAGS "-Wno-error=discarded-qualifiers")
    ADD_COMPILE_FLAGS(
      ecdh.c
      COMPILE_FLAGS "-Wno-error=pointer-sign")
  ELSEIF(MY_COMPILER_IS_CLANG)
    # Downgrade to warning, for now.
    ADD_COMPILE_FLAGS(
      assert.c
      cbor.c
      cred.c
      es256.c
      rs256.c
      COMPILE_FLAGS
      "-Wno-incompatible-pointer-types-discards-qualifiers")
    ADD_COMPILE_FLAGS(
      ecdh.c
      COMPILE_FLAGS "-Wno-pointer-sign")
  ELSEIF(MSVC)
    ADD_COMPILE_FLAGS(
      assert.c
      cred.c
      COMPILE_FLAGS "/wd4090")
    ADD_COMPILE_FLAGS(
      ecdh.c
      COMPILE_FLAGS "/wd4057")
  ENDIF()
ENDIF()


# Windows wants only major.minor
IF(WIN32)
  SET(FIDO_LIBRARY_VERSION ${FIDO_MAJOR}.${FIDO_MINOR})
ELSE()
  SET(FIDO_LIBRARY_VERSION ${FIDO_MAJOR}.${FIDO_MINOR}.${FIDO_PATCH})
ENDIF()

IF(WIN32)
  IF(USE_WINHELLO)
    list(APPEND BASE_LIBRARIES webauthn)
  ENDIF()
ENDIF()

# Add our own shared library:
ADD_SHARED_LIBRARY(fido2
  ${FIDO_SOURCES} ${COMPAT_SOURCES}
  SKIP_INSTALL
  VERSION ${FIDO_LIBRARY_VERSION} SOVERSION ${FIDO_MAJOR}
  LINK_LIBRARIES
    cbor
    ${BASE_LIBRARIES}
    ${CRYPTO_LIBRARY}
    ${HIDAPI_LIBRARIES}
    ${UDEV_LIBRARIES}
    ext::zlib
  NO_UNDEFINED
  )

IF(WIN32)
  TARGET_LINK_LIBRARIES(fido2 wsock32 wsock32 ws2_32 bcrypt setupapi hid)
ELSEIF(APPLE)
  TARGET_LINK_LIBRARIES(fido2 "-framework CoreFoundation" "-framework IOKit")
ENDIF()

IF(WITH_AUTHENTICATION_FIDO OR WITH_AUTHENTICATION_CLIENT_PLUGINS)
  INSTALL_PRIVATE_LIBRARY(fido2)
ENDIF()

# On Linux libcrypto.so.1.1 will be installed in the same
# directory as libfido2.so.1
IF(CRYPTO_CUSTOM_LIBRARY)
  ADD_INSTALL_RPATH(fido2 "\$ORIGIN")
ENDIF()

SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(fido2)

RETURN()
################################################################

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

IF(DISABLE_THESE_LINES)
install(FILES fido.h DESTINATION include)
install(DIRECTORY fido DESTINATION include)

if(NOT WIN32)
	configure_file(libfido2.pc.in libfido2.pc @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libfido2.pc"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
ENDIF(DISABLE_THESE_LINES)
