# Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(FEATURE_SET "community" CACHE STRING 
" Selection of features. Options are
 - xsmall : 
 - small: embedded
 - classic: embedded + archive + federated + blackhole 
 - large :  embedded + archive + federated + blackhole + innodb
 - xlarge:  embedded + archive + federated + blackhole + innodb + partition
 - community:  all  features (currently == xlarge)
"
)

SET(FEATURE_SET_xsmall  1)
SET(FEATURE_SET_small   2)
SET(FEATURE_SET_classic 3)
SET(FEATURE_SET_large   5)
SET(FEATURE_SET_xlarge  6)
SET(FEATURE_SET_community 7)

IF(FEATURE_SET)
  STRING(TOLOWER ${FEATURE_SET} feature_set)
  SET(num ${FEATURE_SET_${feature_set}})
  IF(NOT num)
   MESSAGE(FATAL_ERROR "Invalid FEATURE_SET option '${feature_set}'. 
   Should be xsmall, small, classic, large, or community
   ")
  ENDIF()
  SET(WITH_PARTITION_STORAGE_ENGINE OFF)
  IF(num EQUAL FEATURE_SET_xsmall)
    SET(WITH_NONE ON)
  ENDIF()
  
  IF(num GREATER FEATURE_SET_xsmall)
    SET(WITH_EMBEDDED_SERVER ON CACHE BOOL "")
  ENDIF()
  IF(num GREATER FEATURE_SET_small)
    SET(WITH_ARCHIVE_STORAGE_ENGINE  ON)
    SET(WITH_BLACKHOLE_STORAGE_ENGINE ON)
    SET(WITH_FEDERATED_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_classic)
    SET(WITH_INNOBASE_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_large)
    SET(WITH_PARTITION_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_xlarge)
   # OPTION(WITH_ALL ON) 
   # better no set this, otherwise server would be linked 
   # statically with experimental stuff like audit_null
  ENDIF()
  
  # Update cache with current values, remove engines we do not care about
  # from build.
  FOREACH(eng ARCHIVE BLACKHOLE FEDERATED INNOBASE PARTITION)
    IF(WITHOUT_${eng}_STORAGE_ENGINE)
      SET(WITH_${eng}_STORAGE_ENGINE OFF)
      SET(WITH_${eng}_STORAGE_ENGINE OFF CACHE BOOL "")
    ELSEIF(NOT WITH_${eng}_STORAGE_ENGINE)
      SET(WITHOUT_${eng}_STORAGE_ENGINE ON CACHE BOOL "")
      MARK_AS_ADVANCED(WITHOUT_${eng}_STORAGE_ENGINE)
      SET(WITH_${eng}_STORAGE_ENGINE OFF CACHE BOOL "")
    ELSE()
     SET(WITH_${eng}_STORAGE_ENGINE ON CACHE BOOL "")
    ENDIF()
  ENDFOREACH()
ENDIF()

IF(NOT WITH_SSL)
  SET(WITH_SSL system CACHE STRING "")
ENDIF()
IF(NOT WITH_ZLIB)
  SET(WITH_ZLIB bundled CACHE STRING "")
ENDIF()
