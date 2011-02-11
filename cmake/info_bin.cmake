# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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


# The sole purpose of this cmake control file is to create the "INFO_BIN" file.

# By having a separate cmake file for this, it is ensured this happens
# only in the build (Unix: "make") phase, not when cmake runs.
# This, in turn, avoids creating stuff in the source directory -
# it should get into the binary directory only.


# Get the macros which the "INFO_*" files.
INCLUDE(${CMAKE_BINARY_DIR}/info_macros.cmake)

# Here is where the action is.
CREATE_INFO_BIN()

