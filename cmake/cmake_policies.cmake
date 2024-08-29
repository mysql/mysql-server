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

# versionadded:: 3.15
# In CMake 3.14 and below, MSVC runtime library selection flags are added to
# the default ``CMAKE_<LANG>_FLAGS_<CONFIG>`` cache entries by CMake
# automatically.
#
# .. note::
#
# Once the policy has taken effect at the top of a project, that choice
# must be used throughout the tree.  In projects that have nested projects
# in subdirectories, be sure to convert everything together.
#
# https://cmake.org/cmake/help/latest/policy/CMP0091.html
# This policy was introduced in CMake version 3.15. It may be set by
# cmake_policy() or cmake_minimum_required(). If it is not set, CMake
# does not warn, and uses OLD behavior.
#
# We have code in cmake/os/Windows.cmake to handle the OLD behaviour.
# Whenever we add 3rd party stuff: be sure to keep the policy.
# Also look for CMAKE_MSVC_RUNTIME_LIBRARY and MSVC_RUNTIME_LIBRARY
# in 3rd party cmake code.
IF(POLICY CMP0091)
  # Explicitly setting it to OLD will issue a warning for some cmake versions,
  # so just keep the default behaviour, see above.
  # CMAKE_POLICY(SET CMP0091 OLD)
ENDIF()

# versionadded:: 3.18
# It is not allowed to create an ``ALIAS`` target with the same name as an
# another target.
# The ``OLD`` behavior for this policy is to allow target overwrite.
# The ``NEW`` behavior of this policy is to prevent target overwriting.
IF(POLICY CMP0107)
  CMAKE_POLICY(SET CMP0107 NEW)
ENDIF()
