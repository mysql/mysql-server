set(SOURCES cbor.c allocators.c cbor/streaming.c cbor/internal/encoders.c cbor/internal/builder_callbacks.c cbor/internal/loaders.c cbor/internal/memory_utils.c cbor/internal/stack.c cbor/internal/unicode.c cbor/encoding.c cbor/serialization.c cbor/arrays.c cbor/common.c cbor/floats_ctrls.c cbor/bytestrings.c cbor/callbacks.c cbor/strings.c cbor/maps.c cbor/tags.c cbor/ints.c)

include(GNUInstallDirs)
include(JoinPaths)
include(CheckFunctionExists)
set(CMAKE_SKIP_BUILD_RPATH FALSE)

if (NOT DEFINED CMAKE_MACOSX_RPATH)
	set(CMAKE_MACOSX_RPATH 0)
endif()

add_library(cbor STATIC ${SOURCES})
target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Explicitly link math.h if necessary
check_function_exists(ldexp LDEXP_AVAILABLE)
if (NOT LDEXP_AVAILABLE)
	target_link_libraries(cbor m)
endif()

include(GenerateExportHeader)
generate_export_header(cbor EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/cbor/cbor_export.h)
target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cbor/cbor_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbor)

if (NOT ${CBOR_VERSION_MAJOR} EQUAL 0)
	MESSAGE(FATAL_ERROR "Change the shared library version scheme to reflect https://github.com/PJK/libcbor/issues/52.")
endif()

set_target_properties(cbor PROPERTIES
		VERSION ${CBOR_VERSION}
		MACHO_COMPATIBILITY_VERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR}.0
		SOVERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR})
