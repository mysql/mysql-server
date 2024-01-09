set(protoc_files
  ${protobuf_SOURCE_DIR}/src/google/protobuf/compiler/main.cc
)

add_executable(protoc ${protoc_files} ${protobuf_version_rc_file})
target_link_libraries(protoc
  libprotoc
  libprotobuf
  ${protobuf_ABSL_USED_TARGETS}
)
add_executable(protobuf::protoc ALIAS protoc)

set_target_properties(protoc PROPERTIES
    VERSION ${protobuf_VERSION})

################################################################

SET_TARGET_PROPERTIES(protoc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)

ADD_OBJDUMP_TARGET(show_protoc "$<TARGET_FILE:protoc>" DEPENDS protoc)

# The target symlink_protobuf_dlls will take care of the required
# symbolic links to make protobuf(-lite) library accessible to all
# kind of modules: binaries, plugins, including its debug builds.
# All targets which link with protobuf(-lite) depend on this target.
# This target will also work when building individual components,
# like router.
IF(TARGET symlink_protobuf_dlls)
  ADD_DEPENDENCIES(protoc symlink_protobuf_dlls)
ENDIF()
