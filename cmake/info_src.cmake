# Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA


# The sole purpose of this cmake control file is to create the "INFO_SRC" file.

# As long as and "git pull" (or "git commit") is followed by a "cmake",
# the call in top level "CMakeLists.txt" is sufficient.
# This file is to provide a separate target for the "make" phase,
# to ensure the git commit hash is correct even after a sequence
#   cmake ; make ; git pull ; make


# Get the macros which handle the "INFO_*" files.
INCLUDE(${CMAKE_BINARY_DIR}/info_macros.cmake)

# Here is where the action is.
CREATE_INFO_SRC(${CMAKE_BINARY_DIR}/Docs)

