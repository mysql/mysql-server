# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

MACRO(MY_ADD_C_WARNING_FLAG WARNING_FLAG)
  MY_CHECK_C_COMPILER_FLAG("-${WARNING_FLAG}" HAVE_${WARNING_FLAG})
  IF(HAVE_${WARNING_FLAG})
    SET(MY_C_WARNING_FLAGS "${MY_C_WARNING_FLAGS} -${WARNING_FLAG}")
  ENDIF()
ENDMACRO()

MACRO(MY_ADD_CXX_WARNING_FLAG WARNING_FLAG)
  STRING(REPLACE "c++" "cpp" WARNING_VAR ${WARNING_FLAG})
  MY_CHECK_CXX_COMPILER_FLAG("-${WARNING_FLAG}" HAVE_${WARNING_VAR})
  IF(HAVE_${WARNING_VAR})
    SET(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} -${WARNING_FLAG}")
  ENDIF()
ENDMACRO()

#
# Common flags for all versions/compilers
#

# Common warning flags for GCC, G++, Clang and Clang++
SET(MY_WARNING_FLAGS
    "-Wall -Wextra -Wformat-security -Wvla -Wundef")

# Gives spurious warnings on 32-bit; see GCC bug 81890.
IF(SIZEOF_VOIDP EQUAL 8)
  SET(MY_WARNING_FLAGS "${MY_WARNING_FLAGS} -Wmissing-format-attribute")
ENDIF()

# Common warning flags for GCC and Clang
SET(MY_C_WARNING_FLAGS
    "${MY_WARNING_FLAGS} -Wwrite-strings")

# Common warning flags for G++ and Clang++
SET(MY_CXX_WARNING_FLAGS "${MY_WARNING_FLAGS} -Woverloaded-virtual")

# GCC bug #36750 (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36750)
# Remove when we require GCC >= 5.1 everywhere.
if(CMAKE_COMPILER_IS_GNUCXX)
  MY_ADD_CXX_WARNING_FLAG("Wno-missing-field-initializers")
ENDIF()

# The default =3 given by -Wextra is a bit too strict for our code.
IF(CMAKE_COMPILER_IS_GNUCXX)
  MY_ADD_CXX_WARNING_FLAG("Wimplicit-fallthrough=2")
ENDIF()

#
# Extra flags not supported on all versions/compilers
#

# Only for C++ as C code has some macro usage that is difficult to avoid
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  MY_ADD_CXX_WARNING_FLAG("Wlogical-op")
ENDIF()

# Extra warning flags for Clang
IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(MY_C_WARNING_FLAGS
      "${MY_C_WARNING_FLAGS} -Wconditional-uninitialized")
  SET(MY_C_WARNING_FLAGS
      "${MY_C_WARNING_FLAGS} -Wextra-semi")
  SET(MY_C_WARNING_FLAGS
      "${MY_C_WARNING_FLAGS} -Wmissing-noreturn")

  MY_ADD_C_WARNING_FLAG("Wunreachable-code-break")
  MY_ADD_C_WARNING_FLAG("Wunreachable-code-return")
  # Other possible options that give warnings (Clang 3.8):
  # -Wcast-align
  # -Wcast-qual
  # -Wconversion
  # -Wcovered-switch-default
  # -Wdisabled-macro-expansion
  # -Wdocumentation-deprecated-sync
  # -Wdocumentation-pedantic
  # -Wdocumentation-unknown-command
  # -Wdouble-promotion
  # -Wempty-translation-unit
  # -Wfloat-equal
  # -Wformat-nonlitera
  # -Wformat-pedantic
  # -Wmissing-prototypes
  # -Wmissing-variable-declarations
  # -Wpadded
  # -Wpedantic
  # -Wreserved-id-macro
  # -Wshadow
  # -Wshorten-64-to-32
  # -Wsign-conversion
  # -Wswitch-enum
  # -Wunreachable-code
  # -Wunused-macros
  # -Wused-but-marked-unused
ENDIF()    
  
# Extra warning flags for Clang++
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Disable a few default Clang++ warnings
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wno-null-conversion -Wno-unused-private-field")

  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wconditional-uninitialized")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wheader-hygiene")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wnon-virtual-dtor")
  SET(MY_CXX_WARNING_FLAGS
      "${MY_CXX_WARNING_FLAGS} -Wundefined-reinterpret-cast")
  # Other possible options that give warnings (Clang 3.8):
  # -Wabstract-vbase-init
  # -Wc++98-compat-pedantic
  # -Wcast-align
  # -Wconversion
  # -Wcovered-switch-default
  # -Wdeprecated
  # -Wdisabled-macro-expansion
  # -Wdocumentation
  # -Wdocumentation-pedantic
  # -Wdocumentation-unknown-command
  # -Wdouble-promotion
  # -Wexit-time-destructors
  # -Wextended-offsetof
  # -Wextra-semi
  # -Wfloat-equal
  # -Wformat-nonliteral
  # -Wformat-pedantic
  # -Wglobal-constructors
  # -Wgnu-anonymous-struct
  # -Wimplicit-fallthrough
  # -Wkeyword-macro
  # -Wmissing-noreturn
  # -Wmissing-prototypes
  # -Wmissing-variable-declarations
  # -Wnested-anon-types
  # -Wnewline-eof
  # -Wold-style-cast
  # -Wpadded
  # -Wpedantic
  # -Wreserved-id-macro
  # -Wshadow
  # -Wshift-sign-overflow
  # -Wsign-conversion
  # -Wswitch-enum
  # -Wunreachable-code
  # -Wunreachable-code-break
  # -Wunreachable-code-return
  # -Wunused-exception-parameter
  # -Wunused-macros
  # -Wunused-member-function
  # -Wused-but-marked-unused
  # -Wweak-template-vtables
  # -Wweak-vtables
ENDIF()

# Turn on Werror (warning => error) when using maintainer mode.
IF(MYSQL_MAINTAINER_MODE)
  STRING_APPEND(MY_C_WARNING_FLAGS   " -Werror")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Werror")
ENDIF()

# Set warning flags for GCC/Clang
IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  STRING_APPEND(CMAKE_C_FLAGS " ${MY_C_WARNING_FLAGS}")
ENDIF()
# Set warning flags for G++/Clang++
IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${MY_CXX_WARNING_FLAGS}")
ENDIF()
