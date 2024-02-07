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

MACRO(MY_ADD_C_WARNING_FLAG WARNING_FLAG)
  MY_CHECK_C_COMPILER_FLAG("-${WARNING_FLAG}" HAVE_${WARNING_FLAG})
  IF(HAVE_${WARNING_FLAG})
    STRING_APPEND(MY_C_WARNING_FLAGS " -${WARNING_FLAG}")
  ENDIF()
ENDMACRO()

MACRO(MY_ADD_CXX_WARNING_FLAG WARNING_FLAG)
  MY_CHECK_CXX_COMPILER_WARNING("-${WARNING_FLAG}" HAS_FLAG)
  IF(HAS_FLAG)
    STRING_APPEND(MY_CXX_WARNING_FLAGS " -${WARNING_FLAG}")
  ENDIF()
ENDMACRO()

MACRO(DISABLE_DOCUMENTATION_WARNINGS)
  STRING(REPLACE "-Wdocumentation" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  STRING(REPLACE "-Wdocumentation" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
ENDMACRO()

#
# Common flags for all versions/compilers
#

# Common warning flags for GCC, G++, Clang and Clang++
SET(MY_WARNING_FLAGS
  "-Wall -Wextra -Wformat-security -Wvla -Wundef -Wmissing-format-attribute")

# Clang 6.0 and newer on Windows treat -Wall as -Weverything; use /W4 instead
IF(WIN32_CLANG)
  STRING(REPLACE "-Wall -Wextra" "/W4" MY_WARNING_FLAGS "${MY_WARNING_FLAGS}")
ENDIF()

# Common warning flags for GCC and Clang
SET(MY_C_WARNING_FLAGS "${MY_WARNING_FLAGS} -Wwrite-strings")

# Common warning flags for G++ and Clang++
SET(MY_CXX_WARNING_FLAGS "${MY_WARNING_FLAGS} -Woverloaded-virtual -Wcast-qual")

IF(MY_COMPILER_IS_GNU)
  # Accept only the standard [[fallthrough]] attribute, no comments.
  MY_ADD_CXX_WARNING_FLAG("Wimplicit-fallthrough=5")
  MY_ADD_C_WARNING_FLAG("Wjump-misses-init")
  # This is included in -Wall on some platforms, enable it explicitly.
  MY_ADD_C_WARNING_FLAG("Wstringop-truncation")
  MY_ADD_CXX_WARNING_FLAG("Wstringop-truncation")
  MY_ADD_CXX_WARNING_FLAG("Wsuggest-override")
  MY_ADD_C_WARNING_FLAG("Wmissing-include-dirs")
  MY_ADD_CXX_WARNING_FLAG("Wmissing-include-dirs")

  MY_ADD_CXX_WARNING_FLAG("Wextra-semi") # For gcc8 and up
ENDIF()

#
# Extra flags not supported on all versions/compilers
#

# Only for C++ as C code has some macro usage that is difficult to avoid
IF(MY_COMPILER_IS_GNU)
  MY_ADD_CXX_WARNING_FLAG("Wlogical-op")
ENDIF()

# Extra warning flags for Clang/Clang++
IF(MY_COMPILER_IS_CLANG)
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wconditional-uninitialized")
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wextra-semi")
  STRING_APPEND(MY_C_WARNING_FLAGS " -Wmissing-noreturn")

  MY_ADD_C_WARNING_FLAG("Wunreachable-code-break")
  MY_ADD_C_WARNING_FLAG("Wunreachable-code-return")
  MY_ADD_C_WARNING_FLAG("Wstring-concatenation")

  IF(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
    # require clang-11 or later when enabling -Wdocumentation to workaround
    #
    # https://bugs.llvm.org/show_bug.cgi?id=38905
    MY_ADD_C_WARNING_FLAG("Wdocumentation")

    # -Wdocumentation enables -Wdocumentation-deprecated-sync
    # which currently raises to many warnings
    MY_ADD_C_WARNING_FLAG("Wno-documentation-deprecated-sync")
  ENDIF()

  # Disable a few default Clang++ warnings
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wno-null-conversion")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wno-unused-private-field")

  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wconditional-uninitialized")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wdeprecated")

  # Xcode >= 14 makes noise about sprintf, and loss of precision when
  # assigning integers from 64 bits to 32 bits, so silence. We can't
  # put these two deprecation exceptions in Darwin.cmake because the
  # previous line adding -Wdeprecated would be added later and
  # override them, so do it here instead:
  IF(APPLE)
     STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wno-deprecated-declarations")
     STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wno-shorten-64-to-32")
  ENDIF()

  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wextra-semi")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wheader-hygiene")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wnon-virtual-dtor")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wundefined-reinterpret-cast")
  STRING_APPEND(MY_CXX_WARNING_FLAGS " -Wrange-loop-analysis")

  MY_ADD_CXX_WARNING_FLAG("Winconsistent-missing-destructor-override")
  MY_ADD_CXX_WARNING_FLAG("Winconsistent-missing-override")
  MY_ADD_CXX_WARNING_FLAG("Wshadow-field")
  MY_ADD_CXX_WARNING_FLAG("Wstring-concatenation")

  # require clang-11 or later when enabling -Wdocumentation to workaround
  #
  # https://bugs.llvm.org/show_bug.cgi?id=38905
  MY_ADD_CXX_WARNING_FLAG("Wdocumentation")
  # -Wdocumentation enables -Wdocumentation-deprecated-sync
  # which currently raises to many warnings
  MY_ADD_CXX_WARNING_FLAG("Wno-documentation-deprecated-sync")

  # Other possible options that give warnings (Clang 6.0):
  # -Wabstract-vbase-init
  # -Wc++2a-compat
  # -Wc++98-compat-pedantic
  # -Wcast-align
  # -Wclass-varargs
  # -Wcomma
  # -Wconversion
  # -Wcovered-switch-default
  # -Wdeprecated-dynamic-exception-spec
  # -Wdisabled-macro-expansion
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
  # -Wkeyword-macro
  # -Wmissing-noreturn
  # -Wmissing-prototypes
  # -Wmissing-variable-declarations
  # -Wnested-anon-types
  # -Wnewline-eof
  # -Wold-style-cast
  # -Wpadded
  # -Wpedantic
  # -Wredundant-parens
  # -Wreserved-id-macro
  # -Wshadow
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
  IF(MSVC)
    STRING_APPEND(CMAKE_C_FLAGS   " /WX")
    STRING_APPEND(CMAKE_CXX_FLAGS " /WX")
    STRING_APPEND(CMAKE_EXE_LINKER_FLAGS    " /WX")
    STRING_APPEND(CMAKE_MODULE_LINKER_FLAGS " /WX")
    STRING_APPEND(CMAKE_SHARED_LINKER_FLAGS " /WX")
  ENDIF()
  IF(MY_COMPILER_IS_GNU_OR_CLANG)
    STRING_APPEND(MY_C_WARNING_FLAGS   " -Werror")
    STRING_APPEND(MY_CXX_WARNING_FLAGS " -Werror")
  ENDIF()
ENDIF()

# Set warning flags for gcc/g++/clang/clang++
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  STRING_APPEND(CMAKE_C_FLAGS   " ${MY_C_WARNING_FLAGS}")
  STRING_APPEND(CMAKE_CXX_FLAGS " ${MY_CXX_WARNING_FLAGS}")
ENDIF()

MACRO(ADD_WSHADOW_WARNING)
  IF(MY_COMPILER_IS_GNU)
    ADD_COMPILE_OPTIONS("-Wshadow=local")
  ELSEIF(MY_COMPILER_IS_CLANG)
    # added in clang-5.0
    ADD_COMPILE_OPTIONS("-Wshadow-uncaptured-local")
  ENDIF()
ENDMACRO()
