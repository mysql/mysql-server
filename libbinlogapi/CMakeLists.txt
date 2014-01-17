INCLUDE(configure.cmake)
# General settings
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/libbinlogapi/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/libbinlogapi/lib)

# Locate Google Test package and enable tests if it is found
find_package(GTest ${GTEST_VERSION} QUIET)

# Options for building
option(WITH_SERVER_TESTS
  "Build the unit test suite with tests requiring a server"
  OFF)

if(GTEST_FOUND)
  message(STATUS "Tests from subdirectory 'tests' added")
  enable_testing(true)
  include_directories(${GTEST_INCLUDE_DIRS})
endif(GTEST_FOUND)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
# ZLIB_INCLUDE_DIR points to ${CMAKE_SOURCE_DIR}/zlib, when compiled with
# MySQL server version (< 5.7). It is NULL otherwise.
  ${ZLIB_INCLUDE_DIR}
)

message(status "The zlib paths are ${ZLIB_INCLUDE_DIR} and ${ZLIB_INCLUDE_DIRS}")
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif(ZLIB_FOUND) 

CONFIGURE_FILE(config.h.cmake
               ${CMAKE_SOURCE_DIR}/libbinlogapi/include/config.h)

add_subdirectory(src)

# Configure installation
if (NOT HAVE_MYSYS)
install(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h")
endif (NOT HAVE_MYSYS)
include(InstallRequiredSystemLibraries)

