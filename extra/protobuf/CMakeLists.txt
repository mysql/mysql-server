# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This cmake file builds protoc, the protobuf compiler.
# We also build libprotobuf.a and libprotobuf-lite.a, the protobuf runtime
# support libraries.

# The files in protobuf-2.6.1/ are unmodified versions of google source files.
# To save some space and time, we have remove directories which are not
# needed by MySQL:
#   protobuf-2.6.1/gtest/
#   protobuf-2.6.1/java/
#   protobuf-2.6.1/python/
#   protobuf-2.6.1/m4/
#   protobuf-2.6.1/vsprojects/

SET(PROTOBUF_MSVC_DISABLED_WARNINGS "/wd4018 /wd4005 /wd4244 /wd4267 /wd4065")

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

# Turn off some warning flags when compiling protobuf
MY_CHECK_CXX_COMPILER_FLAG("-Wno-sign-compare" HAVE_NO_SIGN_COMPARE)
IF(HAVE_NO_SIGN_COMPARE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-local-typedefs" HAVE_NO_UNUSED_TYPEDEFS)
IF(HAVE_NO_UNUSED_TYPEDEFS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-ignored-qualifiers" HAVE_NO_IGNORED_QUALIFIERS)
IF(HAVE_NO_IGNORED_QUALIFIERS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-qualifiers")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-return-type" HAVE_NO_RETURN_TYPE)
IF(HAVE_NO_RETURN_TYPE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-type")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wno-unused-function" HAVE_NO_UNUSED_FUNCTION)
IF(HAVE_NO_UNUSED_FUNCTION)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wmaybe-uninitialized" HAVE_MAYBE_UNINITIALIZED)
IF(HAVE_MAYBE_UNINITIALIZED)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
ENDIF()

MY_CHECK_CXX_COMPILER_FLAG("-Wunused-but-set-parameter" HAVE_UNUSED_BUT_SET)
IF(HAVE_UNUSED_BUT_SET)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-parameter")
ENDIF()

ADD_DEFINITIONS(-DHAVE_ZLIB)
IF(CMAKE_USE_PTHREADS_INIT)
  ADD_DEFINITIONS(-DHAVE_PTHREAD)
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/protobuf-2.6.1/src
)

SET(PROTO_SRC_DIR
  "${CMAKE_SOURCE_DIR}/extra/protobuf/protobuf-2.6.1/src")

SET(LIBPROTOBUF_LITE_SOURCES
  ${PROTO_SRC_DIR}/google/protobuf/stubs/atomicops_internals_x86_gcc.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/common.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/once.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/hash.h
  ${PROTO_SRC_DIR}/google/protobuf/stubs/map_util.h
  ${PROTO_SRC_DIR}/google/protobuf/stubs/shared_ptr.h
  ${PROTO_SRC_DIR}/google/protobuf/stubs/stringprintf.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/stringprintf.h
  ${PROTO_SRC_DIR}/google/protobuf/extension_set.cc
  ${PROTO_SRC_DIR}/google/protobuf/generated_message_util.cc
  ${PROTO_SRC_DIR}/google/protobuf/message_lite.cc
  ${PROTO_SRC_DIR}/google/protobuf/repeated_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/wire_format_lite.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/coded_stream.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/coded_stream_inl.h
  ${PROTO_SRC_DIR}/google/protobuf/io/zero_copy_stream.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/zero_copy_stream_impl_lite.cc
)

IF(MSVC)
  ADD_COMPILE_FLAGS(${LIBPROTOBUF_LITE_SOURCES} COMPILE_FLAGS ${PROTOBUF_MSVC_DISABLED_WARNINGS})
ENDIF(MSVC)

ADD_CONVENIENCE_LIBRARY(protobuf-lite ${LIBPROTOBUF_LITE_SOURCES})
SET_PROPERTY(TARGET protobuf-lite PROPERTY EXCLUDE_FROM_ALL TRUE)


SET(LIBPROTOBUF_SOURCES
  ${PROTO_SRC_DIR}/google/protobuf/stubs/strutil.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/strutil.h
  ${PROTO_SRC_DIR}/google/protobuf/stubs/substitute.cc
  ${PROTO_SRC_DIR}/google/protobuf/stubs/substitute.h
  ${PROTO_SRC_DIR}/google/protobuf/stubs/structurally_valid.cc
  ${PROTO_SRC_DIR}/google/protobuf/descriptor.cc
  ${PROTO_SRC_DIR}/google/protobuf/descriptor.pb.cc
  ${PROTO_SRC_DIR}/google/protobuf/descriptor_database.cc
  ${PROTO_SRC_DIR}/google/protobuf/dynamic_message.cc
  ${PROTO_SRC_DIR}/google/protobuf/extension_set_heavy.cc
  ${PROTO_SRC_DIR}/google/protobuf/generated_message_reflection.cc
  ${PROTO_SRC_DIR}/google/protobuf/message.cc
  ${PROTO_SRC_DIR}/google/protobuf/reflection_ops.cc
  ${PROTO_SRC_DIR}/google/protobuf/service.cc
  ${PROTO_SRC_DIR}/google/protobuf/text_format.cc
  ${PROTO_SRC_DIR}/google/protobuf/unknown_field_set.cc
  ${PROTO_SRC_DIR}/google/protobuf/wire_format.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/gzip_stream.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/printer.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/strtod.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/tokenizer.cc
  ${PROTO_SRC_DIR}/google/protobuf/io/zero_copy_stream_impl.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/importer.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/parser.cc
)

IF(MSVC)
  ADD_COMPILE_FLAGS(${LIBPROTOBUF_SOURCES} COMPILE_FLAGS ${PROTOBUF_MSVC_DISABLED_WARNINGS})
ENDIF(MSVC)

ADD_CONVENIENCE_LIBRARY(protobuf 
  ${LIBPROTOBUF_SOURCES} ${LIBPROTOBUF_LITE_SOURCES})
SET_PROPERTY(TARGET protobuf PROPERTY EXCLUDE_FROM_ALL TRUE)
TARGET_LINK_LIBRARIES(protobuf
  ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARY} ${LIBRT})

SET(LIBPROTOC_SOURCES
  ${PROTO_SRC_DIR}/google/protobuf/compiler/code_generator.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/command_line_interface.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/plugin.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/plugin.pb.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/subprocess.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/subprocess.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/zip_writer.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/zip_writer.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_enum.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_enum.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_enum_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_enum_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_extension.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_extension.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_file.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_file.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_generator.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_helpers.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_helpers.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_message.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_message.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_message_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_message_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_options.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_primitive_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_primitive_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_service.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_service.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_string_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/cpp/cpp_string_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_context.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_context.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_enum.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_enum.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_enum_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_enum_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_extension.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_extension.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_file.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_file.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_generator.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_generator_factory.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_generator_factory.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_helpers.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_helpers.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_lazy_message_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_lazy_message_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_message.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_message.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_message_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_message_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_name_resolver.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_name_resolver.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_primitive_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_primitive_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_shared_code_generator.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_shared_code_generator.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_service.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_service.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_string_field.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_string_field.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_doc_comment.cc
  ${PROTO_SRC_DIR}/google/protobuf/compiler/java/java_doc_comment.h
  ${PROTO_SRC_DIR}/google/protobuf/compiler/python/python_generator.cc
)

IF(MSVC)
  ADD_COMPILE_FLAGS(${LIBPROTOC_SOURCES} COMPILE_FLAGS ${PROTOBUF_MSVC_DISABLED_WARNINGS})
ENDIF(MSVC)

ADD_CONVENIENCE_LIBRARY(protoclib ${LIBPROTOC_SOURCES})
SET_PROPERTY(TARGET protoclib PROPERTY EXCLUDE_FROM_ALL TRUE)
ADD_EXECUTABLE(protoc ${PROTO_SRC_DIR}/google/protobuf/compiler/main.cc)
SET_PROPERTY(TARGET protoc PROPERTY EXCLUDE_FROM_ALL TRUE)
TARGET_LINK_LIBRARIES(protoc protobuf protoclib protobuf)
