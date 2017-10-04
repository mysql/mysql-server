# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA


# Parameters:
# TARGET_DIR         - path where "protobuf_lite" directory will be created
# PROTO_FILE         - path to proto file being converted to protobuf-lite version
# PROTO_FILE_FLAGS   - (optional) comma-separated list of flags used for file processing:
#                      "// ifdef FLAG: ..." is replaced with "..." if "FLAG" is defined

GET_FILENAME_COMPONENT(PROTO_FILE_WD ${PROTO_FILE} NAME)

FILE(READ ${PROTO_FILE} PROTO_FILE_CONTENTS)

IF(PROTO_FILE_FLAGS)
  STRING(REPLACE "," "|" PROTO_FILE_FLAGS ${PROTO_FILE_FLAGS})
  FOREACH(LINE ${PROTO_FILE_CONTENTS})
    STRING(REGEX REPLACE
      "([\\t ]*)//[\\t ]*ifdef[\\t ]+(${PROTO_FILE_FLAGS})[\\t ]*:[\\t ]*(.*)" "\\1\\3"
      LINE ${LINE}
    )
    LIST(APPEND NEW_PROTO_FILE_CONTENTS "${LINE}")
  ENDFOREACH()
ELSE()
  SET(NEW_PROTO_FILE_CONTENTS ${PROTO_FILE_CONTENTS})
ENDIF()

STRING(REGEX REPLACE
  "//[\\t ]+ifndef[\\t ]+${PROTO_FILE_FLAGS}.*//[\\t ]+endif[\\t ]*" ""
  NEW_PROTO_FILE_CONTENTS "${NEW_PROTO_FILE_CONTENTS}")

STRING(REGEX REPLACE
  "(\r*\n)([^\r\n]*//[\\t ]+comment_out_if[\\t ]+${PROTO_FILE_FLAGS})" "\\1// \\2"
  NEW_PROTO_FILE_CONTENTS "${NEW_PROTO_FILE_CONTENTS}")

FILE(MAKE_DIRECTORY "${TARGET_DIR}")
FILE(WRITE "${TARGET_DIR}/${PROTO_FILE_WD}" "${NEW_PROTO_FILE_CONTENTS}")
