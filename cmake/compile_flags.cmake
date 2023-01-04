# Copyright (c) 2014, 2023, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA


## ADD_COMPILE_FLAGS(<source files> COMPILE_FLAGS <flags>)
## Use this for adding compiler flags to source files.
FUNCTION(ADD_COMPILE_FLAGS)
  SET(FILES "")
  SET(FLAGS "")
  SET(COMPILE_FLAGS_SEEN)
  FOREACH(ARG ${ARGV})
    IF(ARG STREQUAL "COMPILE_FLAGS")
      SET(COMPILE_FLAGS_SEEN 1)
    ELSEIF(COMPILE_FLAGS_SEEN)
      LIST(APPEND FLAGS ${ARG})
      IF(${ARG} MATCHES "^-D")
        MESSAGE(WARNING
          "${ARG} should be in COMPILE_DEFINITIONS not COMPILE_FLAGS")
      ENDIF()
    ELSE()
      LIST(APPEND FILES ${ARG})
    ENDIF()
  ENDFOREACH()
  FOREACH(FILE ${FILES})
    FOREACH(FLAG ${FLAGS})
      GET_SOURCE_FILE_PROPERTY(PROP ${FILE} COMPILE_FLAGS)
      IF(NOT PROP)
        SET(PROP ${FLAG})
      ELSE()
        STRING_APPEND(PROP " ${FLAG}")
      ENDIF()
      SET_SOURCE_FILES_PROPERTIES(
        ${FILE} PROPERTIES COMPILE_FLAGS "${PROP}"
        )
    ENDFOREACH()
  ENDFOREACH()
ENDFUNCTION(ADD_COMPILE_FLAGS)


## MY_ADD_COMPILE_DEFINITIONS(<source files> COMPILE_DEFINITIONS <flags>)
## Use this for adding preprocessor flags VAR or VAR=value to source files.
## cmake will prefix with '-D' and sort all COMPILE_DEFINITIONS alphabetically.
FUNCTION(MY_ADD_COMPILE_DEFINITIONS)
  SET(FILES "")
  SET(FLAGS "")
  SET(COMPILE_DEFINITIONS_SEEN)
  FOREACH(ARG ${ARGV})
    IF(ARG STREQUAL "COMPILE_DEFINITIONS")
      SET(COMPILE_DEFINITIONS_SEEN 1)
    ELSEIF(COMPILE_DEFINITIONS_SEEN)
      LIST(APPEND FLAGS ${ARG})
    ELSE()
      LIST(APPEND FILES ${ARG})
    ENDIF()
  ENDFOREACH()
  FOREACH(FILE ${FILES})
    GET_SOURCE_FILE_PROPERTY(DEFS ${FILE} COMPILE_DEFINITIONS)
    IF(NOT DEFS)
      SET(DEFS ${FLAGS})
    ELSE()
      LIST(APPEND DEFS ${FLAGS})
    ENDIF()
    SET_SOURCE_FILES_PROPERTIES(
      ${FILE} PROPERTIES COMPILE_DEFINITIONS "${DEFS}")
  ENDFOREACH()
ENDFUNCTION(MY_ADD_COMPILE_DEFINITIONS)

# -flto[=n] or -flto=auto or -flto=jobserver
SET(MY_COMPILER_FLAG_FLTO " -flto(=[0-9a-z]+)?")

# Remove compiler flag/pattern from CMAKE_C_FLAGS or CMAKE_CXX_FLAGS
FUNCTION(REMOVE_CMAKE_COMPILER_FLAGS FLAG_VAR PATTERN)
  STRING(REGEX REPLACE "${PATTERN}" "" ${FLAG_VAR} "${${FLAG_VAR}}")
  SET(${FLAG_VAR} "${${FLAG_VAR}}" PARENT_SCOPE)
ENDFUNCTION()

# Set CMAKE_C_FLAGS and CMAKE_CXX_FLAGS to 'rpm --eval %optflags'
FUNCTION(ADD_LINUX_RPM_FLAGS)
  EXECUTE_PROCESS(COMMAND ${MY_RPM} --eval %optflags
    OUTPUT_VARIABLE RPM_EVAL_OPTFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RPM_EVAL_RESULT
    )
  IF(RPM_EVAL_RESULT EQUAL 0)
    STRING_APPEND(CMAKE_C_FLAGS " ${RPM_EVAL_OPTFLAGS}")
    STRING_APPEND(CMAKE_CXX_FLAGS " ${RPM_EVAL_OPTFLAGS}")
    SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}" PARENT_SCOPE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
  ELSE()
    MESSAGE(FATAL_ERROR
      "WITH_PACKAGE_FLAGS=on but rpm --eval %optflags failed")
  ENDIF()
ENDFUNCTION(ADD_LINUX_RPM_FLAGS)

# Set CMAKE_C_FLAGS and CMAKE_CXX_FLAGS to
#   dpkg-buildflags --get <lang>FLAGS CPPFLAGS
# Set CMAKE_EXE_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS and
# CMAKE_SHARED_LINKER_FLAGS to
#   dpkg-buildflags --get LDFLAGS
FUNCTION(ADD_LINUX_DEB_FLAGS)
  FOREACH(flag CPPFLAGS CFLAGS CXXFLAGS LDFLAGS)
    EXECUTE_PROCESS(COMMAND ${MY_DPKG_BUILDFLAGS} --get ${flag}
      OUTPUT_VARIABLE GET_${flag}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE GET_RESULT
      )
    IF(NOT GET_RESULT EQUAL 0)
      MESSAGE(FATAL_ERROR
        "WITH_PACKAGE_FLAGS=on but dpkg-buildflags --get failed")
    ENDIF()
    SET(CMAKE_C_FLAGS   "${GET_CFLAGS}   ${GET_CPPFLAGS}" PARENT_SCOPE)
    SET(CMAKE_CXX_FLAGS "${GET_CXXFLAGS} ${GET_CPPFLAGS}" PARENT_SCOPE)
    SET(CMAKE_EXE_LINKER_FLAGS    "${GET_LDFLAGS}" PARENT_SCOPE)
    SET(CMAKE_MODULE_LINKER_FLAGS "${GET_LDFLAGS}" PARENT_SCOPE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${GET_LDFLAGS}" PARENT_SCOPE)
  ENDFOREACH()
ENDFUNCTION(ADD_LINUX_DEB_FLAGS)

# See if we can do "-fuse-ld=${LINKER}" for gcc/clang on Linux.
# If compilation/linking succeeds, we extend misc cmake LINKER_FLAGS,
# and set OUTPUT_RESULT to 1.
FUNCTION(CHECK_ALTERNATIVE_LINKER LINKER OUTPUT_RESULT)
  CMAKE_PUSH_CHECK_STATE(RESET)

  SET(CMAKE_REQUIRED_LIBRARIES "-fuse-ld=${LINKER}")
  CHECK_C_SOURCE_COMPILES("int main() {}" C_LD_${LINKER}_RESULT)
  CHECK_CXX_SOURCE_COMPILES("int main() {}" CXX_LD_${LINKER}_RESULT)
  IF(C_LD_${LINKER}_RESULT AND CXX_LD_${LINKER}_RESULT)
    FOREACH(flag
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        )
      STRING_APPEND(${flag} " -fuse-ld=${LINKER}")
      SET(${flag} ${${flag}} PARENT_SCOPE)
    ENDFOREACH()
    SET(${OUTPUT_RESULT} 1 PARENT_SCOPE)
  ELSE()
    SET(${OUTPUT_RESULT} 0 PARENT_SCOPE)
    MESSAGE(STATUS "Cannot use ${LINKER} on this platform")
  ENDIF()

  CMAKE_POP_CHECK_STATE()
ENDFUNCTION(CHECK_ALTERNATIVE_LINKER)
