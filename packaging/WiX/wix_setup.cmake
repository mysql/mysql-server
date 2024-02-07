# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# First we look for "wix.exe" using the CMake variable WIX_DIR, second using the
# environment variable WIX, and then we add "$ENV{USERPROFILE}\\.dotnet\\tools"
# to the PATH before searching the other default locations
IF(NOT WIX_DIR AND "$ENV{WIX}")
  SET(WIX_DIR "$ENV{WIX}")
ENDIF()
FIND_PATH(WIX_DIR wix.exe "$ENV{USERPROFILE}\\.dotnet\\tools")

# Just to avoid writing out the same thing 2-3 times
IF(NOT _WIX_DIR_CHECKED)
  SET(_WIX_DIR_CHECKED 1 CACHE INTERNAL "")
  IF(WIX_DIR)
    MESSAGE(STATUS "WIX_DIR ${WIX_DIR}")
  ELSE()
    MESSAGE(STATUS "Cannot find \"wix.exe\", installer project will not be generated")
  ENDIF()
ENDIF()

IF(NOT WIX_DIR)
  RETURN()
ENDIF()

FIND_PROGRAM(WIX_EXECUTABLE wix ${WIX_DIR})

FUNCTION (CREATE_WIX_LICENCE_AND_RTF license_file)
  # WiX wants the license text as rtf; if there is no rtf license,
  # we create a fake one from the plain text LICENSE file.
  IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.rtf")
   SET(LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.rtf" PARENT_SCOPE)
  ELSE()
    FILE(READ ${license_file} CONTENTS)
    STRING(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
    STRING(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
    FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
    FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "${CONTENTS}")
    FILE(APPEND "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "\n}\n")
    SET(LICENSE_RTF "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" PARENT_SCOPE)
  ENDIF()
ENDFUNCTION()
