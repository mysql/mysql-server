# The extraction of libcno should be done in subfolder, that
# has either commit-id or version in name like:
#
#     "libcno-VERSION".
#
# Currently, those should ve commits-ids.
#
# The git repo uses 'picohttpparser' as submodule, which is problematic
# because `git archive` doesn't include that.
# User must fetch that library/archive and extract it under:
#  ./libcno-VERSION/picohttpparser/
#

SET(LIBCNO_VERSION_DIR "libcno-208939f540957a35b337dacdd5c5e34d51821bd2")
SET(LIBCNO_GENERATE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

DISABLE_MISSING_PROFILE_WARNING()

# Following targets were created to mimic behavior of following
# files supplied by libcno:
#
#    ./cno/hpack-data.py
#    ./cno/Makefile
#
# The output of this application is a C header file, which
# is used by 'libcno' internally. The file is not included directly
# in MySQL source code, because its big and hard to read.
#
MYSQL_ADD_EXECUTABLE(
  cno_huffman_generator
  cno_huffman_generator.cc
  SKIP_INSTALL
  EXCLUDE_FROM_ALL
  )
ADD_CUSTOM_COMMAND(
  OUTPUT "${LIBCNO_GENERATE_DIR}/hpack-data.h"
  COMMAND cno_huffman_generator
  ARGS "${LIBCNO_GENERATE_DIR}/hpack-data.h"
  DEPENDS
  cno_huffman_generator
  COMMENT "LIBCNO generating huffman codes for HTTP2 encodings."
  VERBATIM
  )
MY_ADD_CUSTOM_TARGET(do_cno_huffman_generator
  DEPENDS
  cno_huffman_generator
  "${LIBCNO_GENERATE_DIR}/hpack-data.h"
  SOURCES
  "${LIBCNO_GENERATE_DIR}/hpack-data.h"
)

ADD_LIBRARY(cno STATIC
  ${LIBCNO_VERSION_DIR}/cno/common.c
  ${LIBCNO_VERSION_DIR}/cno/core.c
  ${LIBCNO_VERSION_DIR}/cno/hpack.c
  ${LIBCNO_VERSION_DIR}/picohttpparser/picohttpparser.c
  )
TARGET_INCLUDE_DIRECTORIES(cno
  SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${LIBCNO_VERSION_DIR}/
  PRIVATE ${LIBCNO_GENERATE_DIR}
  )
ADD_DEPENDENCIES(cno
  do_cno_huffman_generator
  )
