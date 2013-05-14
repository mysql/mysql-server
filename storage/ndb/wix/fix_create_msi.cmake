# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

# This is a meta script which copies packaging/WiX/create_msi.cmake into ndb/wix 
# and patches it so that it will run locally as a build step without failing.

# Read the original script
FILE(READ "${SRC_SCRIPT}" C_SRC_SCRIPT)

# The original script runs cmake on the cmake cache (the binary dir) to 
# modify the value of the variable CPACK_MONOLITHIC_INSTALL. This is needed
# since the top-level server CMakeLists.txt hard-codes this to 1, and it needs to be 0 to get the
# components-based install needed by installers. But doing it inside the script makes a mess of 
# cmake's dependency mechanisms because after modifying the cache value all VS projects are re-created,
# so that all targets are out of date. So the modification of the cache is now done as two separate target, 
# and the modifiaction in the script is disabled by this hack: The call to cmake to modify the 
# cache is transformed to a harmless echo command. 
STRING(REPLACE "-DCPACK_MONOLITHIC_INSTALL=" "-E echo " C_COMP_SCRIPT "${C_SRC_SCRIPT}")

# The original script also calls the WiX tools candle and light and tests for successful execution. 
# But the wxs file resulting when using a MySQL Cluster install is broken and will result in failures 
# and warnings from these tools. To avoid build failures we remove the RESULT_VARIABLE and add OUTPUT_QUIET
STRING(REPLACE "RESULT_VARIABLE CANDLE_RESULT" "OUTPUT_QUIET" C_CANDLE_QUIET_SCRIPT "${C_COMP_SCRIPT}")
STRING(REPLACE "RESULT_VARIABLE LIGHT_RESULT" "OUTPUT_QUIET" C_LIGHT_QUIET_SCRIPT "${C_CANDLE_QUIET_SCRIPT}")

# Write out the modified script 
FILE(WRITE "${FIXED_SCRIPT}" "${C_LIGHT_QUIET_SCRIPT}")

# Append a script needed to write the intermediate wxs.in file 
FILE(READ "${ADD_SCRIPT}" C_ADD_SCRIPT)
FILE(APPEND "${FIXED_SCRIPT}" "${C_ADD_SCRIPT}")