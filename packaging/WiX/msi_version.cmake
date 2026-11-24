# Copyright (c) 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License, version 2.0, for more details.

INCLUDE_GUARD(GLOBAL)

# Windows Installer compares only three version fields. Reserve two decimal
# digits of its build field for the update. Callers must supply all components
# from the same product version.
FUNCTION(MYSQL_COMPUTE_MSI_VERSION prefix major minor patch update)
  FOREACH(component major minor patch update)
    IF(NOT "${${component}}" MATCHES "^(0|[1-9][0-9]*)$")
      MESSAGE(FATAL_ERROR "MSI version component ${component} must be a "
        "whole number without leading zeroes.")
    ENDIF()
  ENDFOREACH()

  IF(major GREATER 255 OR minor GREATER 255)
    MESSAGE(FATAL_ERROR "MSI major and minor version components must not exceed 255.")
  ENDIF()
  IF(update GREATER 99)
    MESSAGE(FATAL_ERROR "MSI update version must not exceed 99.")
  ENDIF()
  # Bound the patch before multiplication to avoid arithmetic overflow.
  IF(patch GREATER 655)
    MESSAGE(FATAL_ERROR "MSI build version (patch * 100 + update) must not exceed 65535.")
  ENDIF()
  MATH(EXPR build_version "${patch} * 100 + ${update}")
  IF(build_version GREATER 65535)
    MESSAGE(FATAL_ERROR "MSI build version (patch * 100 + update) must not exceed 65535.")
  ENDIF()

  SET(${prefix}_VERSION "${major}.${minor}.${build_version}" PARENT_SCOPE)
  SET(${prefix}_PATCH_VERSION "${build_version}" PARENT_SCOPE)
  SET(${prefix}_UPGRADE_MIN_VERSION "${major}.${minor}.0" PARENT_SCOPE)
ENDFUNCTION()
