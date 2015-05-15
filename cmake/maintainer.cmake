# Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

MACRO(MY_ADD_C_WARNING_FLAG WARNING_FLAG)
  MY_CHECK_C_COMPILER_FLAG("-${WARNING_FLAG}" HAVE_${WARNING_FLAG})
  IF(HAVE_${WARNING_FLAG})
    SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -${WARNING_FLAG}")
  ENDIF()
ENDMACRO()

# Common warning flags for GCC, G++, Clang and Clang++
SET(MY_WARNING_FLAGS "-Wall -Wextra -Wformat-security -Wvla")

# Common warning flags for GCC and Clang
SET(MY_C_WARNING_FLAGS
    "${MY_WARNING_FLAGS} -Wwrite-strings -Wdeclaration-after-statement")

# Common warning flags for G++ and Clang++
SET(MY_CXX_WARNING_FLAGS
    "${MY_WARNING_FLAGS} -Woverloaded-virtual -Wno-unused-parameter")

# Turn on extra Clang warnings in maintainer mode
IF(CMAKE_C_COMPILER_ID MATCHES "Clang" AND MYSQL_MAINTAINER_MODE)
  MY_ADD_C_WARNING_FLAG("Wconditional-uninitialized")
  MY_ADD_C_WARNING_FLAG("Wunreachable-code-return")
  MY_ADD_C_WARNING_FLAG("Wunreachable-code-break")
  MY_ADD_C_WARNING_FLAG("Wextra-semi")
  MY_ADD_C_WARNING_FLAG("Wmissing-noreturn")
# Other possible options that give warnings:
# -Wcast-align -Wsign-conversion -Wcast-qual -Wreserved-id-macro -Wundef
# -Wdocumentation-unknown-command -Wpadded -Wconversion -Wshorten-64-to-32
# -Wmissing-prototypes -Wused-but-marked-unused -Wmissing-variable-declarations
# -Wdocumentation -Wunreachable-code -Wunused-macros -Wformat-nonliteral
# -Wbad-function-cast -Wcovered-switch-default -Wfloat-equal -Wshadow -Wswitch-enum
# -Wdisabled-macro-expansion -Wpacked -Wpedantic
ENDIF()    
  
# Extra warning flags for Clang++
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wno-null-conversion -Wno-unused-private-field")

# Other possible options that give warnings:
# -Wold-style-cast -Wundef -Wc++11-long-long -Wconversion -Wsign-conversion -Wcast-align
# -Wmissing-prototypes -Wdocumentation -Wweak-vtables -Wdocumentation-unknown-command
# -Wreserved-id-macro -Wpadded -Wused-but-marked-unused -Wshadow -Wunreachable-code-return
# -Wunused-macros -Wmissing-variable-declarations -Wc++11-extra-semi -Wswitch-enum
# -Wextra-semi -Wfloat-equal -Wnon-virtual-dtor -Wmissing-noreturn -Wcovered-switch-default
# -Wunreachable-code-break -Wglobal-constructors -Wc99-extensions -Wshift-sign-overflow
# -Wformat-nonliteral -Wexit-time-destructors -Wpedantic -Wconditional-uninitialized
# -Wunreachable-code-loop-increment -Wdisabled-macro-expansion -Wunreachable-code
# -Wc++11-compat-reserved-user-defined-literal -Wundefined-reinterpret-cast
# -Wabstract-vbase-init -Wclass-varargs -Wover-aligned -Wunused-exception-parameter
# -Wunused-member-function -Wheader-hygiene")
ENDIF()

# Turn on Werror (warning => error) when using maintainer mode.
IF(MYSQL_MAINTAINER_MODE)
  SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -Werror")
  SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -Werror")
ENDIF()

# Set warning flags for GCC/Clang
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_WARNING_FLAGS}")
ENDIF()
# Set warning flags for G++/Clang++
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")
ENDIF()
