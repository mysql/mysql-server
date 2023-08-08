# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

list(APPEND COMPAT_SOURCES
	../openbsd-compat/clock_gettime.c
	../openbsd-compat/getopt_long.c
	../openbsd-compat/strlcat.c
	../openbsd-compat/strlcpy.c
)

if(WIN32 AND BUILD_SHARED_LIBS AND NOT CYGWIN AND NOT MSYS)
	list(APPEND COMPAT_SOURCES ../openbsd-compat/posix_win.c)
endif()

# enable -Wconversion -Wsign-conversion
if(NOT MSVC)
	set_source_files_properties(assert.c cred.c info.c manifest.c reset.c
	    retries.c setpin.c util.c PROPERTIES COMPILE_FLAGS
	    "-Wconversion -Wsign-conversion")
endif()

# manifest
add_executable(manifest manifest.c ${COMPAT_SOURCES})
target_link_libraries(manifest ${_FIDO2_LIBRARY})

# info
add_executable(info info.c ${COMPAT_SOURCES})
target_link_libraries(info ${_FIDO2_LIBRARY})

# reset
add_executable(reset reset.c util.c ${COMPAT_SOURCES})
target_link_libraries(reset ${_FIDO2_LIBRARY})

# cred
add_executable(cred cred.c util.c ${COMPAT_SOURCES})
target_link_libraries(cred ${_FIDO2_LIBRARY})

# assert
add_executable(assert assert.c util.c ${COMPAT_SOURCES})
target_link_libraries(assert ${_FIDO2_LIBRARY})

# setpin
add_executable(setpin setpin.c ${COMPAT_SOURCES})
target_link_libraries(setpin ${_FIDO2_LIBRARY})

# retries
add_executable(retries retries.c ${COMPAT_SOURCES})
target_link_libraries(retries ${_FIDO2_LIBRARY})

# select
add_executable(select select.c ${COMPAT_SOURCES})
target_link_libraries(select ${_FIDO2_LIBRARY})

if(MINGW)
	# needed for nanosleep() in mingw
	target_link_libraries(select winpthread)
endif()
