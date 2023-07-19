#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################
include(CheckCCompilerFlag)

if(PICKY_COMPILER)
  if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")

    # https://clang.llvm.org/docs/DiagnosticsReference.html
    # https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html

    # WPICKY_ENABLE = Options we want to enable as-is.
    # WPICKY_DETECT = Options we want to test first and enable if available.

    # Prefer the -Wextra alias with clang.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      set(WPICKY_ENABLE "-Wextra")
    else()
      set(WPICKY_ENABLE "-W")
    endif()

    list(APPEND WPICKY_ENABLE
      -Wall -pedantic
    )

    # ----------------------------------
    # Add new options here, if in doubt:
    # ----------------------------------
    set(WPICKY_DETECT
    )

    # Assume these options always exist with both clang and gcc.
    # Require clang 3.0 / gcc 2.95 or later.
    list(APPEND WPICKY_ENABLE
      -Wbad-function-cast                  # clang  3.0  gcc  2.95
      -Wconversion                         # clang  3.0  gcc  2.95
      -Winline                             # clang  1.0  gcc  1.0
      -Wmissing-declarations               # clang  1.0  gcc  2.7
      -Wmissing-prototypes                 # clang  1.0  gcc  1.0
      -Wnested-externs                     # clang  1.0  gcc  2.7
      -Wno-long-long                       # clang  1.0  gcc  2.95
      -Wno-multichar                       # clang  1.0  gcc  2.95
      -Wpointer-arith                      # clang  1.0  gcc  1.4
      -Wshadow                             # clang  1.0  gcc  2.95
      -Wsign-compare                       # clang  1.0  gcc  2.95
      -Wundef                              # clang  1.0  gcc  2.95
      -Wunused                             # clang  1.1  gcc  2.95
      -Wwrite-strings                      # clang  1.0  gcc  1.4
    )

    # Always enable with clang, version dependent with gcc
    set(WPICKY_COMMON_OLD
      -Wcast-align                         # clang  1.0  gcc  4.2
      -Wdeclaration-after-statement        # clang  1.0  gcc  3.4
      -Wempty-body                         # clang  3.0  gcc  4.3
      -Wendif-labels                       # clang  1.0  gcc  3.3
      -Wfloat-equal                        # clang  1.0  gcc  2.96 (3.0)
      -Wignored-qualifiers                 # clang  3.0  gcc  4.3
      -Wno-format-nonliteral               # clang  1.0  gcc  2.96 (3.0)
      -Wno-sign-conversion                 # clang  3.0  gcc  4.3
      -Wno-system-headers                  # clang  1.0  gcc  3.0
      -Wstrict-prototypes                  # clang  1.0  gcc  3.3
      -Wtype-limits                        # clang  3.0  gcc  4.3
      -Wvla                                # clang  2.8  gcc  4.3
    )

    set(WPICKY_COMMON
      -Wdouble-promotion                   # clang  3.6  gcc  4.6  appleclang  6.3
      -Wenum-conversion                    # clang  3.2  gcc 10.0  appleclang  4.6  g++ 11.0
      -Wunused-const-variable              # clang  3.4  gcc  6.0  appleclang  5.1
    )

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      list(APPEND WPICKY_ENABLE
        ${WPICKY_COMMON_OLD}
        -Wshift-sign-overflow              # clang  2.9
        -Wshorten-64-to-32                 # clang  1.0
      )
      # Enable based on compiler version
      if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 3.6) OR
         (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 6.3))
        list(APPEND WPICKY_ENABLE
          ${WPICKY_COMMON}
        )
      endif()
      if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 3.9) OR
         (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 8.3))
        list(APPEND WPICKY_ENABLE
          -Wcomma                          # clang  3.9            appleclang  8.3
          -Wmissing-variable-declarations  # clang  3.2            appleclang  4.6
        )
      endif()
      if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 7.0) OR
         (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 10.3))
        list(APPEND WPICKY_ENABLE
          -Wassign-enum                    # clang  7.0            appleclang 10.3
          -Wextra-semi-stmt                # clang  7.0            appleclang 10.3
        )
      endif()
    else()  # gcc
      list(APPEND WPICKY_DETECT
        ${WPICKY_COMMON}
      )
      # Enable based on compiler version
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 4.3)
        list(APPEND WPICKY_ENABLE
          ${WPICKY_COMMON_OLD}
          -Wmissing-parameter-type         #             gcc  4.3
          -Wold-style-declaration          #             gcc  4.3
          -Wstrict-aliasing=3              #             gcc  4.0
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 4.5 AND MINGW)
        list(APPEND WPICKY_ENABLE
          -Wno-pedantic-ms-format          #             gcc  4.5 (mingw-only)
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 4.8)
        list(APPEND WPICKY_ENABLE
          -Wformat=2                       # clang  3.0  gcc  4.8 (clang part-default, enabling it fully causes -Wformat-nonliteral warnings)
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0)
        list(APPEND WPICKY_ENABLE
          -Warray-bounds=2 -ftree-vrp      # clang  3.0  gcc  5.0 (clang default: -Warray-bounds)
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 6.0)
        list(APPEND WPICKY_ENABLE
          -Wduplicated-cond                #             gcc  6.0
          -Wnull-dereference               # clang  3.0  gcc  6.0 (clang default)
            -fdelete-null-pointer-checks
          -Wshift-negative-value           # clang  3.7  gcc  6.0 (clang default)
          -Wshift-overflow=2               # clang  3.0  gcc  6.0 (clang default: -Wshift-overflow)
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 7.0)
        list(APPEND WPICKY_ENABLE
          -Walloc-zero                     #             gcc  7.0
          -Wduplicated-branches            #             gcc  7.0
          -Wformat-overflow=2              #             gcc  7.0
          -Wformat-truncation=1            #             gcc  7.0
          -Wrestrict                       #             gcc  7.0
        )
      endif()
      if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 10.0)
        list(APPEND WPICKY_ENABLE
          -Warith-conversion               #             gcc 10.0
        )
      endif()
    endif()

    #

    unset(WPICKY)

    foreach(_CCOPT ${WPICKY_ENABLE})
      set(WPICKY "${WPICKY} ${_CCOPT}")
    endforeach()

    foreach(_CCOPT ${WPICKY_DETECT})
      # surprisingly, CHECK_C_COMPILER_FLAG needs a new variable to store each new
      # test result in.
      string(MAKE_C_IDENTIFIER "OPT${_CCOPT}" _optvarname)
      # GCC only warns about unknown -Wno- options if there are also other diagnostic messages,
      # so test for the positive form instead
      string(REPLACE "-Wno-" "-W" _CCOPT_ON "${_CCOPT}")
      check_c_compiler_flag(${_CCOPT_ON} ${_optvarname})
      if(${_optvarname})
        set(WPICKY "${WPICKY} ${_CCOPT}")
      endif()
    endforeach()

    message(STATUS "Picky compiler options:${WPICKY}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WPICKY}")
  endif()
endif()
