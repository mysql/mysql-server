# Copyright (c) 2018-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

# detect AppleClang; needs to come before project()
cmake_policy(SET CMP0025 NEW)

project(libfido2 C)
cmake_minimum_required(VERSION 3.0)
# Set PIE flags for POSITION_INDEPENDENT_CODE targets, added in CMake 3.14.
if(POLICY CMP0083)
	cmake_policy(SET CMP0083 NEW)
endif()

include(CheckCCompilerFlag)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(GNUInstallDirs)
include(CheckPIESupported OPTIONAL RESULT_VARIABLE CHECK_PIE_SUPPORTED)
if(CHECK_PIE_SUPPORTED)
	check_pie_supported(LANGUAGES C)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COLOR_MAKEFILE OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(FIDO_MAJOR "1")
set(FIDO_MINOR "13")
set(FIDO_PATCH "0")
set(FIDO_VERSION ${FIDO_MAJOR}.${FIDO_MINOR}.${FIDO_PATCH})

option(BUILD_TESTS       "Build the regress tests"                 ON)
option(BUILD_EXAMPLES    "Build example programs"                  ON)
option(BUILD_MANPAGES    "Build man pages"                         ON)
option(BUILD_SHARED_LIBS "Build a shared library"                  ON)
option(BUILD_STATIC_LIBS "Build a static library"                  ON)
option(BUILD_TOOLS       "Build tool programs"                     ON)
option(FUZZ              "Enable fuzzing instrumentation"          OFF)
option(USE_HIDAPI        "Use hidapi as the HID backend"           OFF)
option(USE_PCSC          "Enable experimental PCSC support"        OFF)
option(USE_WINHELLO      "Abstract Windows Hello as a FIDO device" ON)
option(NFC_LINUX         "Enable NFC support on Linux"             ON)

add_definitions(-D_FIDO_MAJOR=${FIDO_MAJOR})
add_definitions(-D_FIDO_MINOR=${FIDO_MINOR})
add_definitions(-D_FIDO_PATCH=${FIDO_PATCH})

if(BUILD_SHARED_LIBS)
	set(_FIDO2_LIBRARY fido2_shared)
elseif(BUILD_STATIC_LIBS)
	set(_FIDO2_LIBRARY fido2)
else()
	message(FATAL_ERROR "Nothing to build (BUILD_*_LIBS=OFF)")
endif()

if(CYGWIN OR MSYS OR MINGW)
	set(WIN32 1)
endif()

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0600)
endif()

if(APPLE)
	set(CMAKE_INSTALL_NAME_DIR
	    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(NOT MSVC)
	set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_POSIX_C_SOURCE=200809L")
	set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_BSD_SOURCE")
	if(APPLE)
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_DARWIN_C_SOURCE")
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D__STDC_WANT_LIB_EXT1__=1")
	elseif((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR MINGW OR CYGWIN)
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_GNU_SOURCE")
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_DEFAULT_SOURCE")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR
	    CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D__BSD_VISIBLE=1")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
		set(FIDO_CFLAGS "${FIDO_CFLAGS} -D_NETBSD_SOURCE")
	endif()
	set(FIDO_CFLAGS "${FIDO_CFLAGS} -std=c99")
	set(CMAKE_C_FLAGS "${FIDO_CFLAGS} ${CMAKE_C_FLAGS}")
endif()

check_c_compiler_flag("-Wshorten-64-to-32" HAVE_SHORTEN_64_TO_32)
check_c_compiler_flag("-Werror -fstack-protector-all" HAVE_STACK_PROTECTOR_ALL)

check_include_files(cbor.h HAVE_CBOR_H)
check_include_files(endian.h HAVE_ENDIAN_H)
check_include_files(err.h HAVE_ERR_H)
check_include_files(openssl/opensslv.h HAVE_OPENSSLV_H)
check_include_files(signal.h HAVE_SIGNAL_H)
check_include_files(sys/random.h HAVE_SYS_RANDOM_H)
check_include_files(unistd.h HAVE_UNISTD_H)

check_symbol_exists(arc4random_buf stdlib.h HAVE_ARC4RANDOM_BUF)
check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
check_symbol_exists(explicit_bzero string.h HAVE_EXPLICIT_BZERO)
check_symbol_exists(freezero stdlib.h HAVE_FREEZERO)
check_symbol_exists(getline stdio.h HAVE_GETLINE)
check_symbol_exists(getopt unistd.h HAVE_GETOPT)
check_symbol_exists(getpagesize unistd.h HAVE_GETPAGESIZE)
check_symbol_exists(getrandom sys/random.h HAVE_GETRANDOM)
check_symbol_exists(memset_s string.h HAVE_MEMSET_S)
check_symbol_exists(readpassphrase readpassphrase.h HAVE_READPASSPHRASE)
check_symbol_exists(recallocarray stdlib.h HAVE_RECALLOCARRAY)
check_symbol_exists(strlcat string.h HAVE_STRLCAT)
check_symbol_exists(strlcpy string.h HAVE_STRLCPY)
check_symbol_exists(strsep string.h HAVE_STRSEP)
check_symbol_exists(sysconf unistd.h HAVE_SYSCONF)
check_symbol_exists(timespecsub sys/time.h HAVE_TIMESPECSUB)
check_symbol_exists(timingsafe_bcmp string.h HAVE_TIMINGSAFE_BCMP)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
try_compile(HAVE_POSIX_IOCTL
    "${CMAKE_CURRENT_BINARY_DIR}/posix_ioctl_check.o"
    "${CMAKE_CURRENT_SOURCE_DIR}/openbsd-compat/posix_ioctl_check.c"
    COMPILE_DEFINITIONS "-Werror -Woverflow -Wsign-conversion")

list(APPEND CHECK_VARIABLES
	HAVE_ARC4RANDOM_BUF
	HAVE_ASPRINTF
	HAVE_CBOR_H
	HAVE_CLOCK_GETTIME
	HAVE_ENDIAN_H
	HAVE_ERR_H
	HAVE_FREEZERO
	HAVE_GETLINE
	HAVE_GETOPT
	HAVE_GETPAGESIZE
	HAVE_GETRANDOM
	HAVE_MEMSET_S
	HAVE_OPENSSLV_H
	HAVE_POSIX_IOCTL
	HAVE_READPASSPHRASE
	HAVE_RECALLOCARRAY
	HAVE_SIGNAL_H
	HAVE_STRLCAT
	HAVE_STRLCPY
	HAVE_STRSEP
	HAVE_SYSCONF
	HAVE_SYS_RANDOM_H
	HAVE_TIMESPECSUB
	HAVE_TIMINGSAFE_BCMP
	HAVE_UNISTD_H
)

foreach(v ${CHECK_VARIABLES})
	if (${v})
		add_definitions(-D${v})
	endif()
endforeach()

if(HAVE_EXPLICIT_BZERO AND NOT FUZZ)
	add_definitions(-DHAVE_EXPLICIT_BZERO)
endif()

if(UNIX)
	add_definitions(-DHAVE_DEV_URANDOM)
endif()


if(MSVC)
	if((NOT CBOR_INCLUDE_DIRS) OR (NOT CBOR_LIBRARY_DIRS) OR
	   (NOT CRYPTO_INCLUDE_DIRS) OR (NOT CRYPTO_LIBRARY_DIRS) OR
	   (NOT ZLIB_INCLUDE_DIRS) OR (NOT ZLIB_LIBRARY_DIRS))
		message(FATAL_ERROR "please define "
		   "{CBOR,CRYPTO,ZLIB}_{INCLUDE,LIBRARY}_DIRS when "
		   "building under msvc")
	endif()
	if(BUILD_TESTS AND BUILD_SHARED_LIBS AND
	   ((NOT CBOR_BIN_DIRS) OR (NOT ZLIB_BIN_DIRS) OR (NOT CRYPTO_BIN_DIRS)))
		message(FATAL_ERROR "please define {CBOR,CRYPTO,ZLIB}_BIN_DIRS "
		   "when building tests")
	endif()
	if(NOT CBOR_LIBRARIES)
		set(CBOR_LIBRARIES cbor)
	endif()
	if(NOT ZLIB_LIBRARIES)
		set(ZLIB_LIBRARIES zlib1)
	endif()
	if(NOT CRYPTO_LIBRARIES)
		set(CRYPTO_LIBRARIES crypto)
	endif()

	set(MSVC_DISABLED_WARNINGS_LIST
		"C4152" # nonstandard extension used: function/data pointer
			# conversion in expression;
		"C4200" # nonstandard extension used: zero-sized array in
			# struct/union;
		"C4201" # nonstandard extension used: nameless struct/union;
		"C4204" # nonstandard extension used: non-constant aggregate
			# initializer;
		"C4706" # assignment within conditional expression;
		"C4996" # The POSIX name for this item is deprecated. Instead,
			# use the ISO C and C++ conformant name;
		"C6287" # redundant code: the left and right subexpressions are identical
		)
	# The construction in the following 3 lines was taken from LibreSSL's
	# CMakeLists.txt.
	string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
	    ${MSVC_DISABLED_WARNINGS_LIST})
	string(REGEX REPLACE "[/-]W[1234][ ]?" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MP -W4 -WX ${MSVC_DISABLED_WARNINGS_STR}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /Z7 /guard:cf /sdl /RTCcsu")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi /guard:cf /sdl")
	if(USE_WINHELLO)
		add_definitions(-DUSE_WINHELLO)
	endif()
	set(NFC_LINUX OFF)
else()
	include(FindPkgConfig)
	pkg_search_module(CBOR libcbor)
	pkg_search_module(CRYPTO libcrypto)
	pkg_search_module(ZLIB zlib)

	if(NOT CBOR_FOUND AND NOT HAVE_CBOR_H)
		message(FATAL_ERROR "could not find libcbor")
	endif()
	if(NOT CRYPTO_FOUND AND NOT HAVE_OPENSSLV_H)
		message(FATAL_ERROR "could not find libcrypto")
	endif()
	if(NOT ZLIB_FOUND)
		message(FATAL_ERROR "could not find zlib")
	endif()

	if(NOT CBOR_LIBRARIES)
		set(CBOR_LIBRARIES "cbor")
	endif()
	if(NOT CRYPTO_LIBRARIES)
		set(CRYPTO_LIBRARIES "crypto")
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		pkg_search_module(UDEV libudev REQUIRED)
		set(UDEV_NAME "udev")
		# If using hidapi, use hidapi-hidraw.
		set(HIDAPI_SUFFIX -hidraw)
		if(NOT HAVE_CLOCK_GETTIME)
			# Look for clock_gettime in librt.
			check_library_exists(rt clock_gettime "time.h"
			    HAVE_CLOCK_GETTIME)
			if (HAVE_CLOCK_GETTIME)
				add_definitions(-DHAVE_CLOCK_GETTIME)
				set(BASE_LIBRARIES ${BASE_LIBRARIES} rt)
			endif()
		endif()
	else()
		set(NFC_LINUX OFF)
	endif()

	if(MINGW)
		# MinGW is stuck with a flavour of C89.
		add_definitions(-DFIDO_NO_DIAGNOSTIC)
		add_definitions(-DWC_ERR_INVALID_CHARS=0x80)
		add_compile_options(-Wno-unused-parameter)
	endif()

	if(FUZZ)
		set(USE_PCSC ON)
		add_definitions(-DFIDO_FUZZ)
	endif()

	# If building with PCSC, look for pcsc-lite.
	if(USE_PCSC AND NOT (APPLE OR CYGWIN OR MSYS OR MINGW))
		pkg_search_module(PCSC libpcsclite REQUIRED)
		set(PCSC_LIBRARIES pcsclite)
	endif()

	if(USE_HIDAPI)
		add_definitions(-DUSE_HIDAPI)
		pkg_search_module(HIDAPI hidapi${HIDAPI_SUFFIX} REQUIRED)
		set(HIDAPI_LIBRARIES hidapi${HIDAPI_SUFFIX})
	endif()

	if(NFC_LINUX)
		add_definitions(-DUSE_NFC)
	endif()

	if(WIN32)
		if(USE_WINHELLO)
			add_definitions(-DUSE_WINHELLO)
		endif()
	else()
		set(USE_WINHELLO OFF)
	endif()

	add_compile_options(-Wall)
	add_compile_options(-Wextra)
	add_compile_options(-Werror)
	add_compile_options(-Wshadow)
	add_compile_options(-Wcast-qual)
	add_compile_options(-Wwrite-strings)
	add_compile_options(-Wmissing-prototypes)
	add_compile_options(-Wbad-function-cast)
	add_compile_options(-Wimplicit-fallthrough)
	add_compile_options(-pedantic)
	add_compile_options(-pedantic-errors)

	set(EXTRA_CFLAGS "-Wconversion -Wsign-conversion")

	if(WIN32)
		add_compile_options(-Wno-type-limits)
		add_compile_options(-Wno-cast-function-type)
	endif()

	if(HAVE_SHORTEN_64_TO_32)
		add_compile_options(-Wshorten-64-to-32)
	endif()

	if(HAVE_STACK_PROTECTOR_ALL)
		add_compile_options(-fstack-protector-all)
	endif()

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g2")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")

	if(CRYPTO_VERSION VERSION_GREATER_EQUAL 3.0)
		add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)
	endif()

	if(NOT FUZZ)
		set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -Wframe-larger-than=2047")
	endif()
endif()

# Avoid https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
if(CMAKE_COMPILER_IS_GNUCC)
	add_compile_options(-Wno-unused-result)
endif()

# Decide which keyword to use for thread-local storage.
if(CMAKE_COMPILER_IS_GNUCC OR
   CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
	set(TLS "__thread")
elseif(WIN32)
	set(TLS "__declspec(thread)")
endif()
add_definitions(-DTLS=${TLS})

if(USE_PCSC)
	add_definitions(-DUSE_PCSC)
endif()

# export list
if(APPLE AND (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_C_COMPILER_ID STREQUAL "AppleClang"))
	# clang + lld
	string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
	    " -exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/src/export.llvm")
elseif(NOT MSVC)
	# clang/gcc + gnu ld
	if(FUZZ)
		string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
		    " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/export.gnu")
	else()
		string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
		    " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/export.gnu")
	endif()
	if(NOT WIN32)
		string(CONCAT CMAKE_SHARED_LINKER_FLAGS
		    ${CMAKE_SHARED_LINKER_FLAGS}
		    " -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
		string(CONCAT CMAKE_EXE_LINKER_FLAGS
		    ${CMAKE_EXE_LINKER_FLAGS}
		    " -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
		if(FUZZ)
			file(STRINGS fuzz/wrapped.sym WRAPPED_SYMBOLS)
			foreach(s ${WRAPPED_SYMBOLS})
				string(CONCAT CMAKE_SHARED_LINKER_FLAGS
				    ${CMAKE_SHARED_LINKER_FLAGS}
				    " -Wl,--wrap=${s}")
			endforeach()
		endif()
	endif()
else()
	string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
	    " /def:\"${CMAKE_CURRENT_SOURCE_DIR}/src/export.msvc\"")
endif()

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${CBOR_INCLUDE_DIRS})
include_directories(${CRYPTO_INCLUDE_DIRS})
include_directories(${HIDAPI_INCLUDE_DIRS})
include_directories(${PCSC_INCLUDE_DIRS})
include_directories(${UDEV_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})

link_directories(${CBOR_LIBRARY_DIRS})
link_directories(${CRYPTO_LIBRARY_DIRS})
link_directories(${HIDAPI_LIBRARY_DIRS})
link_directories(${PCSC_LIBRARY_DIRS})
link_directories(${UDEV_LIBRARY_DIRS})
link_directories(${ZLIB_LIBRARY_DIRS})

message(STATUS "BASE_LIBRARIES: ${BASE_LIBRARIES}")
message(STATUS "BUILD_EXAMPLES: ${BUILD_EXAMPLES}")
message(STATUS "BUILD_MANPAGES: ${BUILD_MANPAGES}")
message(STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_STATIC_LIBS: ${BUILD_STATIC_LIBS}")
message(STATUS "BUILD_TOOLS: ${BUILD_TOOLS}")
message(STATUS "CBOR_INCLUDE_DIRS: ${CBOR_INCLUDE_DIRS}")
message(STATUS "CBOR_LIBRARIES: ${CBOR_LIBRARIES}")
message(STATUS "CBOR_LIBRARY_DIRS: ${CBOR_LIBRARY_DIRS}")
if(BUILD_TESTS)
	message(STATUS "CBOR_BIN_DIRS: ${CBOR_BIN_DIRS}")
endif()
message(STATUS "CBOR_VERSION: ${CBOR_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_COMPILER_ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CROSSCOMPILING: ${CMAKE_CROSSCOMPILING}")
message(STATUS "CMAKE_GENERATOR_PLATFORM: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")
message(STATUS "CRYPTO_INCLUDE_DIRS: ${CRYPTO_INCLUDE_DIRS}")
message(STATUS "CRYPTO_LIBRARIES: ${CRYPTO_LIBRARIES}")
message(STATUS "CRYPTO_LIBRARY_DIRS: ${CRYPTO_LIBRARY_DIRS}")
if(BUILD_TESTS)
	message(STATUS "CRYPTO_BIN_DIRS: ${CRYPTO_BIN_DIRS}")
endif()
message(STATUS "CRYPTO_VERSION: ${CRYPTO_VERSION}")
message(STATUS "FIDO_VERSION: ${FIDO_VERSION}")
message(STATUS "FUZZ: ${FUZZ}")
if(FUZZ)
	message(STATUS "FUZZ_LDFLAGS: ${FUZZ_LDFLAGS}")
endif()
message(STATUS "ZLIB_INCLUDE_DIRS: ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
message(STATUS "ZLIB_LIBRARY_DIRS: ${ZLIB_LIBRARY_DIRS}")
if(BUILD_TESTS)
	message(STATUS "ZLIB_BIN_DIRS: ${ZLIB_BIN_DIRS}")
endif()
message(STATUS "ZLIB_VERSION: ${ZLIB_VERSION}")
if(USE_HIDAPI)
	message(STATUS "HIDAPI_INCLUDE_DIRS: ${HIDAPI_INCLUDE_DIRS}")
	message(STATUS "HIDAPI_LIBRARIES: ${HIDAPI_LIBRARIES}")
	message(STATUS "HIDAPI_LIBRARY_DIRS: ${HIDAPI_LIBRARY_DIRS}")
	message(STATUS "HIDAPI_VERSION: ${HIDAPI_VERSION}")
endif()
message(STATUS "PCSC_INCLUDE_DIRS: ${PCSC_INCLUDE_DIRS}")
message(STATUS "PCSC_LIBRARIES: ${PCSC_LIBRARIES}")
message(STATUS "PCSC_LIBRARY_DIRS: ${PCSC_LIBRARY_DIRS}")
message(STATUS "PCSC_VERSION: ${PCSC_VERSION}")
message(STATUS "TLS: ${TLS}")
message(STATUS "UDEV_INCLUDE_DIRS: ${UDEV_INCLUDE_DIRS}")
message(STATUS "UDEV_LIBRARIES: ${UDEV_LIBRARIES}")
message(STATUS "UDEV_LIBRARY_DIRS: ${UDEV_LIBRARY_DIRS}")
message(STATUS "UDEV_RULES_DIR: ${UDEV_RULES_DIR}")
message(STATUS "UDEV_VERSION: ${UDEV_VERSION}")
message(STATUS "USE_HIDAPI: ${USE_HIDAPI}")
message(STATUS "USE_PCSC: ${USE_PCSC}")
message(STATUS "USE_WINHELLO: ${USE_WINHELLO}")
message(STATUS "NFC_LINUX: ${NFC_LINUX}")

if(BUILD_TESTS)
	enable_testing()
endif()

add_subdirectory(src)

if(BUILD_TESTS)
	add_subdirectory(regress)
endif()
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
if(BUILD_TOOLS)
	add_subdirectory(tools)
endif()
if(BUILD_MANPAGES)
	add_subdirectory(man)
endif()

if(NOT WIN32)
	if(FUZZ)
		add_subdirectory(fuzz)
	endif()
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		add_subdirectory(udev)
	endif()
endif()
