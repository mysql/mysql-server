# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# detect AppleClang; needs to come before project()
cmake_policy(SET CMP0025 NEW)

IF(POLICY CMP0075)
# Keep NEW, set in cmake/cmake_policies.cmake
# CMAKE_POLICY(SET CMP0075 OLD)
ENDIF()

project(libfido2 C)
# cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(GNUInstallDirs)

# set(CMAKE_COLOR_MAKEFILE off)
# set(CMAKE_VERBOSE_MAKEFILE on)
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FIDO_MAJOR "1")
set(FIDO_MINOR "5")
set(FIDO_PATCH "0")
set(FIDO_VERSION ${FIDO_MAJOR}.${FIDO_MINOR}.${FIDO_PATCH})

add_definitions(-D_FIDO_MAJOR=${FIDO_MAJOR})
add_definitions(-D_FIDO_MINOR=${FIDO_MINOR})
add_definitions(-D_FIDO_PATCH=${FIDO_PATCH})

if(CYGWIN OR MSYS)
	set(WIN32 1)
	add_definitions(-DWINVER=0x0a00)
endif()

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0600)
endif()

if(APPLE)
	set(CMAKE_INSTALL_NAME_DIR
	    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(MSVC)
  IF(NOT WIN32_CLANG)
	set(MSVC_DISABLED_WARNINGS_LIST
		"C4200" # nonstandard extension used: zero-sized array in
			# struct/union;
		"C4204" # nonstandard extension used: non-constant aggregate
			# initializer;
		"C4706" # assignment within conditional expression;
		"C4996" # The POSIX name for this item is deprecated. Instead,
			# use the ISO C and C++ conformant name
		)
	# The construction in the following 3 lines was taken from LibreSSL's
	# CMakeLists.txt.
	string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
	    ${MSVC_DISABLED_WARNINGS_LIST})
	string(REGEX REPLACE "[/-]W[1234][ ]?" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MP -W4 -WX ${MSVC_DISABLED_WARNINGS_STR}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Z7")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
  ENDIF()
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      FIND_LIBRARY(UDEV_SYSTEM_LIBRARY NAMES udev)
      if(NOT UDEV_SYSTEM_LIBRARY)
        MESSAGE(FATAL_ERROR "Cannot find the libudev library ${UDEV_SYSTEM_LIBRARY}")
      endif()
      SET(UDEV_LIBRARIES ${UDEV_SYSTEM_LIBRARY})
      FIND_PATH(UDEV_INCLUDE_DIRS NAMES libudev.h)
	  include_directories(${UDEV_INCLUDE_DIRS})
      # Define be32toh().
	  add_definitions(-D_GNU_SOURCE)
	  # If using hidapi, use hidapi-hidraw.
	  set(HIDAPI_SUFFIX -hidraw)
	  # Look for clock_gettime in librt.
	  check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
	  if(HAVE_CLOCK_GETTIME)
		set(BASE_LIBRARIES ${BASE_LIBRARIES} rt)
		add_definitions(-DHAVE_CLOCK_GETTIME)
      endif()
    endif()

	if(MINGW)
		# MinGW is stuck with a flavour of C89.
		add_definitions(-DFIDO_NO_DIAGNOSTIC)
		add_definitions(-DWC_ERR_INVALID_CHARS=0x80)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
	endif()

	if(FREEBSD)
	    SET(USE_HIDAPI ON)
        add_definitions(-DUSE_HIDAPI)
        find_library(HIDAPI NAMES hidapi${HIDAPI_SUFFIX})
        set(HIDAPI_LIBRARIES ${HIDAPI})
        find_path(HIDAPI_INCLUDE_DIRS NAMES hidapi/hidapi.h)
        include_directories(${HIDAPI_INCLUDE_DIRS}/hidapi/)
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wbad-function-cast")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic-errors")
	check_c_compiler_flag("-fstack-protector-all" HAVE_STACK_PROTECTOR_ALL)
	if(HAVE_STACK_PROTECTOR_ALL)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
	endif()

	add_definitions(-D_DEFAULT_SOURCE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g2")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")

	if(FUZZ)
		if(LIBFUZZER)
			set(FUZZ_LDFLAGS "-fsanitize=fuzzer")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=fuzzer-no-link")
		endif()
		add_definitions(-DFIDO_FUZZ)
	endif()
endif()

# Use -Wshorten-64-to-32 if available.
check_c_compiler_flag("-Wshorten-64-to-32" HAVE_SHORTEN_64_TO_32)
if(HAVE_SHORTEN_64_TO_32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshorten-64-to-32")
endif()

# Avoid https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")
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

# endian.h
check_include_files(endian.h HAVE_ENDIAN_H)
if(HAVE_ENDIAN_H)
	add_definitions(-DHAVE_ENDIAN_H)
endif()

# err.h
check_include_files(err.h HAVE_ERR_H)
if(HAVE_ERR_H)
	add_definitions(-DHAVE_ERR_H)
endif()

# unistd.h
check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif()

# signal.h
check_include_files(signal.h HAVE_SIGNAL_H)
if(HAVE_SIGNAL_H)
	add_definitions(-DHAVE_SIGNAL_H)
endif()

# sys/random.h
check_include_files(sys/random.h HAVE_SYS_RANDOM_H)
if(HAVE_SYS_RANDOM_H)
	add_definitions(-DHAVE_SYS_RANDOM_H)
endif()

# strlcpy
check_function_exists(strlcpy HAVE_STRLCPY)
if(HAVE_STRLCPY)
	add_definitions(-DHAVE_STRLCPY)
endif()

# strlcat
check_function_exists(strlcpy HAVE_STRLCAT)
if(HAVE_STRLCAT)
	add_definitions(-DHAVE_STRLCAT)
endif()

# recallocarray
check_function_exists(recallocarray HAVE_RECALLOCARRAY)
if(HAVE_RECALLOCARRAY)
	add_definitions(-DHAVE_RECALLOCARRAY)
endif()

# XXX getpagesize is incorrectly detected when cross-compiling
# with mingw on Linux. Avoid.
if(NOT WIN32)
	check_function_exists(getpagesize HAVE_GETPAGESIZE)
endif()
if(HAVE_GETPAGESIZE)
	add_definitions(-DHAVE_GETPAGESIZE)
endif()

# sysconf
check_function_exists(sysconf HAVE_SYSCONF)
if(HAVE_SYSCONF)
	add_definitions(-DHAVE_SYSCONF)
endif()

# memset_s
if(APPLE)
	add_definitions(-D__STDC_WANT_LIB_EXT1__=1)
endif()
check_function_exists(memset_s HAVE_MEMSET_S)
if(HAVE_MEMSET_S)
	add_definitions(-DHAVE_MEMSET_S)
endif()

# explicit_bzero
if(NOT LIBFUZZER)
	check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)
	if(HAVE_EXPLICIT_BZERO)
		add_definitions(-DHAVE_EXPLICIT_BZERO)
	endif()
endif()

# timingsafe_bcmp
check_function_exists(timingsafe_bcmp HAVE_TIMINGSAFE_BCMP)
if(HAVE_TIMINGSAFE_BCMP)
	add_definitions(-DHAVE_TIMINGSAFE_BCMP)
endif()

# readpassphrase
check_function_exists(readpassphrase HAVE_READPASSPHRASE)
if(HAVE_READPASSPHRASE)
	add_definitions(-DHAVE_READPASSPHRASE)
endif()

# getline
check_function_exists(getline HAVE_GETLINE)
if(HAVE_GETLINE)
	add_definitions(-DHAVE_GETLINE)
endif()

# getopt
check_function_exists(getopt HAVE_GETOPT)
if(HAVE_GETOPT)
	add_definitions(-DHAVE_GETOPT)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-qual")
else()
	if(CMAKE_COMPILER_IS_GNUCC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-discarded-qualifiers")
	endif()
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-pointer-types-discards-qualifiers")
	endif()
endif()

# usable sigaction
set(CMAKE_EXTRA_INCLUDE_FILES signal.h)
check_function_exists(sigaction HAVE_SIGACTION)
check_type_size("sig_atomic_t" HAVE_SIG_ATOMIC_T)
if(HAVE_SIGACTION AND (NOT HAVE_SIG_ATOMIC_T STREQUAL ""))
	add_definitions(-DSIGNAL_EXAMPLE)
endif()
set(CMAKE_EXTRA_INCLUDE_FILES)

# arc4random_buf
check_function_exists(arc4random_buf HAVE_ARC4RANDOM_BUF)
if(HAVE_ARC4RANDOM_BUF)
	add_definitions(-DHAVE_ARC4RANDOM_BUF)
endif()

# getrandom
check_function_exists(getrandom HAVE_GETRANDOM)
if(HAVE_GETRANDOM)
	add_definitions(-DHAVE_GETRANDOM)
endif()

# /dev/urandom
if(UNIX)
	add_definitions(-DHAVE_DEV_URANDOM)
endif()

# clock_gettime
if(NOT HAVE_CLOCK_GETTIME)
	check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
	if(HAVE_CLOCK_GETTIME)
		add_definitions(-DHAVE_CLOCK_GETTIME)
	endif()
endif()

# timespecsub
check_symbol_exists(timespecsub sys/time.h HAVE_TIMESPECSUB)
if(HAVE_TIMESPECSUB)
	add_definitions(-DHAVE_TIMESPECSUB)
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

SET(CBOR_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/${CBOR_BUNDLE_SRC_PATH}
                      ${CMAKE_SOURCE_DIR}/${CBOR_BUNDLE_SRC_PATH}/src
                      ${CMAKE_BINARY_DIR}/${CBOR_BUNDLE_SRC_PATH})

SET(CBOR_LIBRARY_DIRS ${CMAKE_BINARY_DIR}/${CBOR_BUNDLE_SRC_PATH})

DISABLE_MISSING_PROFILE_WARNING()
IF(FPROFILE_USE)
  MY_CHECK_CXX_COMPILER_WARNING("-Wcoverage-mismatch" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_C_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDIF()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CBOR_INCLUDE_DIRS})

# This should not be necessary, we know where libcbor.a is,
# and there is no need to set any RUNPATH in libfido2.so
# link_directories(${CBOR_LIBRARY_DIRS})

subdirs(src)
#subdirs(examples)
#subdirs(tools)

# if(NOT WIN32)
# 	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
# 		if(NOT LIBFUZZER AND NOT FUZZ)
# 			subdirs(regress)
# 		endif()
# 	endif()
# 	if(FUZZ)
# 		subdirs(fuzz)
# 	endif()
#
# 	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# 		subdirs(udev)
# 	endif()
# endif()
