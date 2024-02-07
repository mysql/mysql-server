# Copyright (c) 2006, 2024, Oracle and/or its affiliates.
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

# Set various cmake policies. This must be done *after* CMAKE_MINIMUM_REQUIRED.

# Explicitly set CMP0018 and CMP0022 = NEW since newer versions of
# CMake has OLD as default even if OLD is deprecated.
# See cmake --help-policy CMP0018 / CMP0022
CMAKE_POLICY(SET CMP0018 NEW)
CMAKE_POLICY(SET CMP0022 NEW)

# Disallow use of the LOCATION property for build targets.
CMAKE_POLICY(SET CMP0026 NEW)

# Include TARGET_OBJECTS expressions.
CMAKE_POLICY(SET CMP0051 NEW)

# INTERPROCEDURAL_OPTIMIZATION is enforced when enabled (CMake 3.9+)
IF(POLICY CMP0069)
  CMAKE_POLICY(SET CMP0069 NEW)
ENDIF()

# Do not produce ``<tgt>_LIB_DEPENDS`` cache entries to propagate library
# link dependencies. In cmake code, use this instead:
#   GET_TARGET_PROPERTY(TARGET_LIB_DEPENDS ${target} LINK_LIBRARIES)
IF(POLICY CMP0073)
  CMAKE_POLICY(SET CMP0073 NEW)
ENDIF()

# In CMake 3.12 and above, the
#
# * ``check_include_file`` macro in the ``CheckIncludeFile`` module, the
# * ``check_include_file_cxx`` macro in the
#   ``CheckIncludeFileCXX`` module, and the
# * ``check_include_files`` macro in the ``CheckIncludeFiles`` module
#
# now prefer to link the check executable to the libraries listed in the
# ``CMAKE_REQUIRED_LIBRARIES`` variable.
IF(POLICY CMP0075)
  CMAKE_POLICY(SET CMP0075 NEW)
ENDIF()
