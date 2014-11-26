# Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(FEATURE_SET "community" CACHE STRING 
" Selection of features. This option is deprecated"
)

IF(FEATURE_SET AND NOT WITHOUT_SERVER)
  SET(WITH_EMBEDDED_SERVER ON CACHE BOOL "")

  # Set these ON by default. They can be disabled with
  # -DWITHOUT_${eng}_STORAGE_ENGINE
  SET(WITH_ARCHIVE_STORAGE_ENGINE  ON)
  SET(WITH_BLACKHOLE_STORAGE_ENGINE ON)
  SET(WITH_FEDERATED_STORAGE_ENGINE ON)
  SET(WITH_INNOBASE_STORAGE_ENGINE ON)
  SET(WITH_PARTITION_STORAGE_ENGINE ON)

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

SET(WITH_SSL bundled CACHE STRING "")
SET(WITH_ZLIB bundled CACHE STRING "")
