set(protoc_files
  ${protobuf_source_dir}/src/google/protobuf/compiler/main.cc
)

if (MSVC AND NOT WIN32_CLANG)
set(protoc_rc_files
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)
endif()

add_executable(protoc ${protoc_files} ${protoc_rc_files})
target_link_libraries(protoc libprotoc libprotobuf)
add_executable(protobuf::protoc ALIAS protoc)

set_target_properties(protoc PROPERTIES
    VERSION ${protobuf_VERSION})

###
SET_TARGET_PROPERTIES(protoc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)
