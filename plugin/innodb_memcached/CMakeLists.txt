# Copyright (c) 2011, 2022, Oracle and/or its affiliates.
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

IF(WITH_INNODB_MEMCACHED AND UNIX)
  IF(MY_COMPILER_IS_GNU)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-discarded-qualifiers")
  ELSEIF(MY_COMPILER_IS_CLANG)
    STRING_APPEND(CMAKE_C_FLAGS
      " -Wno-incompatible-pointer-types-discards-qualifiers")
  ENDIF()
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-implicit-fallthrough")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-missing-field-initializers")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-sign-compare")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-undef")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-function")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-parameter")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-variable")
  STRING_APPEND(CMAKE_C_FLAGS " -Wno-vla")

  MY_CHECK_C_COMPILER_FLAG("-Wduplicate-decl-specifier"
    HAVE_DUPLICATE_DECL_SPECIFIER)
  IF(HAVE_DUPLICATE_DECL_SPECIFIER)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-duplicate-decl-specifier")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wint-conversion" HAVE_INT_CONVERSION)
  IF(HAVE_INT_CONVERSION)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-int-conversion")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wstringop-overflow" HAVE_STRINGOP_OVERFLOW)
  IF(HAVE_STRINGOP_OVERFLOW)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-stringop-overflow")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wstringop-truncation" HAVE_STRINGOP_TRUNCATION)
  IF(HAVE_STRINGOP_TRUNCATION)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-stringop-truncation")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wsuggest-attribute=format"
    HAVE_SUGGEST_ATTRIBUTE_FORMAT)
  IF(HAVE_SUGGEST_ATTRIBUTE_FORMAT)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-suggest-attribute=format")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wunused-but-set-parameter"
    HAVE_UNUSED_BUT_SET_PARAMETER)
  IF(HAVE_UNUSED_BUT_SET_PARAMETER)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-but-set-parameter")
  ENDIF()

  MY_CHECK_C_COMPILER_FLAG("-Wunused-but-set-variable"
    HAVE_UNUSED_BUT_SET_VARIABLE)
  IF(HAVE_UNUSED_BUT_SET_VARIABLE)
    STRING_APPEND(CMAKE_C_FLAGS " -Wno-unused-but-set-variable")
  ENDIF()

  # -Werror=lto-type-mismatch for misc functions.
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_C_FLAGS "${MY_COMPILER_FLAG_FLTO}")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_CXX_FLAGS "${MY_COMPILER_FLAG_FLTO}")

  DISABLE_MISSING_PROFILE_WARNING()

  ADD_SUBDIRECTORY(innodb_memcache)

  # MYSQL_ADD_PLUGIN may have decided not to build it.
  IF(TARGET innodb_engine)
    ADD_SUBDIRECTORY(daemon_memcached)

    INSTALL(FILES
      "innodb_memcache/scripts/innodb_memcached_config.sql"
      COMPONENT Server
      DESTINATION ${INSTALL_MYSQLSHAREDIR}
      PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
      )
  ENDIF()

ENDIF()
