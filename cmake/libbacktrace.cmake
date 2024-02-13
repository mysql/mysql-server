# Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

SET(VERIFIED_BACKTRACE_PLATFORMS
  FREEBSD
  LINUX
  SOLARIS
  )

UNSET(VERIFIED_BACKTRACE_PLATFORM)
FOREACH(platform ${VERIFIED_BACKTRACE_PLATFORMS})
  IF(${platform})
    MESSAGE(STATUS "Found verified platform ${platform} for libbacktrace")
    SET(VERIFIED_BACKTRACE_PLATFORM ${platform})
    BREAK()
  ENDIF()
ENDFOREACH()

IF(VERIFIED_BACKTRACE_PLATFORM AND NOT WITH_VALGRIND)
  SET(DEFAULT_WITH_EXT_BACKTRACE ON)
ELSE()
  SET(DEFAULT_WITH_EXT_BACKTRACE OFF)
ENDIF()
OPTION(WITH_EXT_BACKTRACE "Use libbacktrace to print stacktraces"
  ${DEFAULT_WITH_EXT_BACKTRACE}
)

FUNCTION(MYSQL_CHECK_BACKTRACE)
  IF(NOT WITH_EXT_BACKTRACE)
    RETURN()
  ENDIF()

  IF(NOT LINUX AND NOT SOLARIS AND NOT FREEBSD)
    MESSAGE(FATAL_ERROR
      "libbacktrace can only be used on linux/solaris/freebsd builds")
  ENDIF()

  IF(VERIFIED_BACKTRACE_PLATFORM)
    MESSAGE(STATUS "Using libbacktrace on ${VERIFIED_BACKTRACE_PLATFORM}")
  ELSE()
    MESSAGE(WARNING "Using libbacktrace on unverified platform")
  ENDIF()

  ADD_SUBDIRECTORY(extra/libbacktrace)
  ADD_LIBRARY(ext::backtrace ALIAS backtrace)
ENDFUNCTION(MYSQL_CHECK_BACKTRACE)
