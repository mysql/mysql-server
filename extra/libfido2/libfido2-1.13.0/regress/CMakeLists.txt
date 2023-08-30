# Copyright (c) 2018-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

add_custom_target(regress)

macro(add_regress_test NAME SOURCES LIB)
	add_executable(${NAME} ${SOURCES})
	add_test(${NAME} ${NAME})
	add_dependencies(regress ${NAME})
	target_link_libraries(${NAME} ${LIB})
endmacro()

if(MSVC AND BUILD_SHARED_LIBS)
	add_custom_command(TARGET regress POST_BUILD
	    COMMAND "${CMAKE_COMMAND}" -E copy
		"${CBOR_BIN_DIRS}/${CBOR_LIBRARIES}.dll"
		"${CRYPTO_BIN_DIRS}/${CRYPTO_LIBRARIES}.dll"
		"${ZLIB_BIN_DIRS}/${ZLIB_LIBRARIES}.dll"
		"$<TARGET_FILE:${_FIDO2_LIBRARY}>"
		"${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(CYGWIN AND BUILD_SHARED_LIBS)
	add_custom_command(TARGET regress POST_BUILD
	    COMMAND "${CMAKE_COMMAND}" -E copy
		"$<TARGET_FILE:${_FIDO2_LIBRARY}>"
		"${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(CMAKE_CROSSCOMPILING OR (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64" AND
    CMAKE_GENERATOR_PLATFORM MATCHES "^ARM.*$"))
	add_custom_command(TARGET regress POST_BUILD
	    COMMAND "${CMAKE_COMMAND}" -E echo
		"Cross-compilation detected. Skipping regress tests.")
else()
	add_custom_command(TARGET regress POST_BUILD
	    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
	    WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

add_regress_test(regress_assert assert.c ${_FIDO2_LIBRARY})
add_regress_test(regress_cred cred.c ${_FIDO2_LIBRARY})
add_regress_test(regress_dev dev.c ${_FIDO2_LIBRARY})
add_regress_test(regress_eddsa eddsa.c ${_FIDO2_LIBRARY})
add_regress_test(regress_es256 es256.c ${_FIDO2_LIBRARY})
add_regress_test(regress_es384 es384.c ${_FIDO2_LIBRARY})
add_regress_test(regress_rs256 rs256.c ${_FIDO2_LIBRARY})
if(BUILD_STATIC_LIBS)
	add_regress_test(regress_compress compress.c fido2)
endif()

if(MINGW)
	# needed for nanosleep() in mingw
	target_link_libraries(regress_dev winpthread)
endif()
