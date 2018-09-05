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

# Clang 6.0 and newer on Windows seems to enable -Weverything; turn off some
# that are way too verbose for us.
IF(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "Clang")
  # We don't need C++98 compatibility.
  MY_ADD_CXX_WARNING_FLAG("Wno-c++98-compat")
  MY_ADD_CXX_WARNING_FLAG("Wno-c++98-compat-pedantic")

  # Also warns on using NULL.
  MY_ADD_CXX_WARNING_FLAG("Wno-zero-as-null-pointer-constant")

  # Should be enabled globally, but too noisy right now.
  MY_ADD_CXX_WARNING_FLAG("Wno-old-style-cast")
  MY_ADD_CXX_WARNING_FLAG("Wno-reserved-id-macro")
  MY_ADD_CXX_WARNING_FLAG("Wno-documentation")
  MY_ADD_CXX_WARNING_FLAG("Wno-documentation-pedantic")
  MY_ADD_CXX_WARNING_FLAG("Wno-documentation-unknown-command")
  MY_ADD_CXX_WARNING_FLAG("Wno-missing-variable-declarations")
  MY_ADD_CXX_WARNING_FLAG("Wno-cast-qual")
  MY_ADD_CXX_WARNING_FLAG("Wno-language-extension-token")
  MY_ADD_CXX_WARNING_FLAG("Wno-shorten-64-to-32")
  MY_ADD_CXX_WARNING_FLAG("Wno-shadow")
  MY_ADD_CXX_WARNING_FLAG("Wno-deprecated")  # FIXME
  MY_ADD_CXX_WARNING_FLAG("Wno-inconsistent-missing-destructor-override")
  MY_ADD_CXX_WARNING_FLAG("Wno-c++2a-compat")
  MY_ADD_CXX_WARNING_FLAG("Wno-macro-redefined")
  MY_ADD_CXX_WARNING_FLAG("Wno-unused-const-variable")
  MY_ADD_CXX_WARNING_FLAG("Wno-gnu-anonymous-struct")

  # Various things we don't want to warn about.
  MY_ADD_CXX_WARNING_FLAG("Wno-global-constructors")
  MY_ADD_CXX_WARNING_FLAG("Wno-exit-time-destructors")
  MY_ADD_CXX_WARNING_FLAG("Wno-undefined-func-template")
  MY_ADD_CXX_WARNING_FLAG("Wno-nonportable-system-include-path")
  MY_ADD_CXX_WARNING_FLAG("Wno-sign-conversion")
  MY_ADD_CXX_WARNING_FLAG("Wno-unused-exception-parameter")
  MY_ADD_CXX_WARNING_FLAG("Wno-missing-prototypes")
  MY_ADD_CXX_WARNING_FLAG("Wno-shadow-field-in-constructor")
  MY_ADD_CXX_WARNING_FLAG("Wno-float-equal")
  MY_ADD_CXX_WARNING_FLAG("Wno-float-conversion")
  MY_ADD_CXX_WARNING_FLAG("Wno-double-promotion")
  MY_ADD_CXX_WARNING_FLAG("Wno-covered-switch-default")
  MY_ADD_CXX_WARNING_FLAG("Wno-used-but-marked-unused")
  MY_ADD_CXX_WARNING_FLAG("Wno-conversion")
  MY_ADD_CXX_WARNING_FLAG("Wno-sign-conversion")
  MY_ADD_CXX_WARNING_FLAG("Wno-microsoft-pure-definition")  # False positives.
  MY_ADD_CXX_WARNING_FLAG("Wno-format-nonliteral")
  MY_ADD_CXX_WARNING_FLAG("Wno-format-pedantic")
  MY_ADD_CXX_WARNING_FLAG("Wno-cast-align")
  MY_ADD_CXX_WARNING_FLAG("Wno-gnu-zero-variadic-macro-arguments")
  MY_ADD_CXX_WARNING_FLAG("Wno-comma")
  MY_ADD_CXX_WARNING_FLAG("Wno-sign-compare")
  MY_ADD_CXX_WARNING_FLAG("Wno-switch-enum")
  MY_ADD_CXX_WARNING_FLAG("Wno-implicit-fallthrough")  # Does not take the same signals as GCC.
  MY_ADD_CXX_WARNING_FLAG("Wno-unused-macros")
  MY_ADD_CXX_WARNING_FLAG("Wno-disabled-macro-expansion")
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
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wconditional-uninitialized")
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wextra-semi")
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wmissing-noreturn")

  MY_ADD_C_WARNING_FLAG("Wunreachable-code-break")
  MY_ADD_C_WARNING_FLAG("Wunreachable-code-return")
ENDIF()
  
# Extra warning flags for Clang++
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Disable a few default Clang++ warnings
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wno-null-conversion -Wno-unused-private-field")

  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wconditional-uninitialized")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wheader-hygiene")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wnon-virtual-dtor")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wundefined-reinterpret-cast")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wextra-semi")

  # Other possible options that give warnings (Clang 6.0):
  # -Wabstract-vbase-init
  # -Wc++2a-compat
  # -Wc++98-compat-pedantic
  # -Wcast-align
  # -Wcast-qual
  # -Wclass-varargs
  # -Wcomma
  # -Wconversion
  # -Wcovered-switch-default
  # -Wdeprecated
  # -Wdeprecated-dynamic-exception-spec
  # -Wdisabled-macro-expansion
  # -Wdocumentation
  # -Wdocumentation-pedantic
  # -Wdocumentation-unknown-command
  # -Wdouble-promotion
  # -Wexit-time-destructors
  # -Wfloat-equal
  # -Wformat-nonliteral
  # -Wformat-pedantic
  # -Wglobal-constructors
  # -Wgnu-anonymous-struct
  # -Wgnu-zero-variadic-macro-arguments
  # -Wimplicit-fallthrough
  # -Winconsistent-missing-destructor-override
  # -Wkeyword-macro
  # -Wmissing-noreturn
  # -Wmissing-prototypes
  # -Wmissing-variable-declarations
  # -Wnested-anon-types
  # -Wnewline-eof
  # -Wold-style-cast
  # -Wpadded
  # -Wpedantic
  # -Wrange-loop-analysis
  # -Wredundant-parens
  # -Wreserved-id-macro
  # -Wshadow
  # -Wshadow-field
  # -Wshift-sign-overflow
  # -Wsign-conversion
  # -Wswitch-enum
  # -Wtautological-type-limit-compare
  # -Wtautological-unsigned-enum-zero-compare
  # -Wundefined-func-template
  # -Wunreachable-code
  # -Wunreachable-code-break
  # -Wunreachable-code-return
  # -Wunused-exception-parameter
  # -Wunused-macros
  # -Wunused-member-function
  # -Wunused-template
  # -Wused-but-marked-unused
  # -Wweak-template-vtables
  # -Wweak-vtables
  # -Wzero-as-null-pointer-constant
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
