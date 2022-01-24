set(SOURCES cbor.c cbor/streaming.c cbor/internal/encoders.c cbor/internal/builder_callbacks.c cbor/internal/loaders.c cbor/internal/memory_utils.c cbor/internal/stack.c cbor/internal/unicode.c cbor/encoding.c cbor/serialization.c cbor/arrays.c cbor/common.c cbor/floats_ctrls.c cbor/bytestrings.c cbor/callbacks.c cbor/strings.c cbor/maps.c cbor/tags.c cbor/ints.c)

include(GNUInstallDirs)
set(CMAKE_SKIP_BUILD_RPATH FALSE)

add_library(cbor STATIC ${SOURCES})

target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include(GenerateExportHeader)
generate_export_header(cbor EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/cbor/cbor_export.h)
target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(cbor PROPERTIES
		VERSION ${CBOR_VERSION}
		MACHO_COMPATIBILITY_VERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR}.0
		SOVERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR})
