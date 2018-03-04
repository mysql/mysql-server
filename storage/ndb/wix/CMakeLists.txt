# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

IF(NOT WIN32)
	# MSI building is not relvant on other platforms
	RETURN()
ENDIF()

# This is copied from packaging/Wix. Not sure if it is possible to build the msi with something 
# other than VisualStudio. It is certainly not tested.
IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()

SET(SRC_SCRIPT "${CMAKE_BINARY_DIR}/packaging/WiX/create_msi.cmake")
SET(ADD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/write_wxs.cmake")
SET(FIXED_SCRIPT _create_msi.cmake)

SET(CMAKE_SCRIPT_SETUP COMMAND set VS_UNICODE_OUTPUT= COMMAND "${CMAKE_COMMAND}" --warn-uninitialized)

IF(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)
	ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mono_off.txt"
		COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/mono_off.txt"
		COMMENT "WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL set - just touch mono_off.txt")
ELSE(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)
	# Turn component-based install on.
	# Using a cmake script to do this is cumbersome and possibly not necessary, but is consitent
	# with how component-based install is turned off after the msi has been built. 
	ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mono_off.txt"
		${CMAKE_SCRIPT_SETUP}
		"-DCACHE_ARGS=-DCPACK_MONOLITHIC_INSTALL=0"
		"-DCACHE_DIR=${CMAKE_BINARY_DIR}"		
		-P "${CMAKE_CURRENT_SOURCE_DIR}/update_cache.cmake"
		COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/mono_off.txt"
		COMMENT "Run cmake on cache to enable component-based install (turn off CPACK_MONOLITHIC_INSTALL)"
		VERBATIM)
ENDIF(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)

# Copy and modify create_msi.cmake from packaging/WiX	
ADD_CUSTOM_COMMAND(OUTPUT "${FIXED_SCRIPT}"
	${CMAKE_SCRIPT_SETUP}
	"-DSRC_SCRIPT=${SRC_SCRIPT}" "-DFIXED_SCRIPT=${FIXED_SCRIPT}" "-DADD_SCRIPT=${ADD_SCRIPT}" 
	-P "${CMAKE_CURRENT_SOURCE_DIR}/fix_create_msi.cmake" 
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fix_create_msi.cmake" 
		"${ADD_SCRIPT}" "${CMAKE_SOURCE_DIR}/packaging/WiX/create_msi.cmake.in" 
		"${CMAKE_CURRENT_BINARY_DIR}/mono_off.txt"
	COMMENT "fix_create_msi.cmake( create_msi.cmake ) -> ${FIXED_SCRIPT}"
	VERBATIM)	

SET(WXS_BASENAME mysql_cluster)

# Run modified script to create intermediate mysql_cluster.wxs.in
# Note that since CONFIGURE_FILE has builtin dependency check which prevents 
# .wxs.in from being generated if no variable has changed, we need to touch it to prevent it from
# always being considered out of date.
ADD_CUSTOM_COMMAND(OUTPUT "${WXS_BASENAME}.wxs.in"
	${CMAKE_SCRIPT_SETUP}
	"-DWXS_BASENAME=${WXS_BASENAME}"
	"${CONFIG_PARAM}"
	-P "${CMAKE_CURRENT_BINARY_DIR}/${FIXED_SCRIPT}"
	COMMAND ${CMAKE_COMMAND} -E touch "${WXS_BASENAME}.wxs.in"
 	DEPENDS "${FIXED_SCRIPT}" "${CMAKE_SOURCE_DIR}/packaging/WiX/mysql_server.wxs.in" 
	COMMENT "${FIXED_SCRIPT}( mysql_server.wxs.in ) -> ${WXS_BASENAME}.wxs.in"
	VERBATIM)

# Run ndb_create_wxs.cmake to create a wxs from intermediate wxs.in that is suitable for MySQL Cluster
ADD_CUSTOM_COMMAND(OUTPUT "${WXS_BASENAME}.wxs"
	${CMAKE_SCRIPT_SETUP}
	"-DTOPDIR=${CMAKE_CURRENT_BINARY_DIR}/testinstall" "-DWXS_BASENAME=${WXS_BASENAME}"
	"-DMAJOR_VERSION=${NDB_VERSION_MAJOR}" "-DMINOR_VERSION=${NDB_VERSION_MINOR}" "-DPATCH_VERSION=${NDB_VERSION_BUILD}" 
	-P "${CMAKE_CURRENT_SOURCE_DIR}/ndb_create_wxs.cmake"
	DEPENDS "${WXS_BASENAME}.wxs.in"
	COMMENT "ndb_create_wxs.cmake( ${WXS_BASENAME}.wxs.in ) -> ${WXS_BASENAME}.wxs"
	VERBATIM)

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(WIX_ARCH x64)
ELSE()
  SET(WIX_ARCH x86)
ENDIF()

# Run WiX-tool candle.exe on .wxs to produce WiX "object code"
ADD_CUSTOM_COMMAND(OUTPUT "${WXS_BASENAME}.wixobj"
	COMMAND "${CANDLE_EXECUTABLE}" -nologo -arch "${WIX_ARCH}" -ext WixUtilExtension "${WXS_BASENAME}.wxs" "$ENV{EXTRA_CANDLE_ARGS}"
	DEPENDS "${WXS_BASENAME}.wxs"
	COMMENT "candle( ${WXS_BASENAME}.wxs ) -> ${WXS_BASENAME}.wixobj"
	VERBATIM)

# Run WiX-tool light.exe on "object code" to produce .msi file
ADD_CUSTOM_COMMAND(OUTPUT "${CPACK_PACKAGE_FILE_NAME}.msi"
	COMMAND "${LIGHT_EXECUTABLE}" -nologo -sw1076 -ss -ext WixUIExtension -ext WixUtilExtension "${WXS_BASENAME}.wixobj" -out "${CPACK_PACKAGE_FILE_NAME}.msi" "$ENV{EXTRA_LIGHT_ARGS}"
	DEPENDS "${WXS_BASENAME}.wixobj" wixca
	COMMENT "light( ${WXS_BASENAME}.wixobj ) -> ${CPACK_PACKAGE_FILE_NAME}.msi"
	VERBATIM)

# Set WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL=YES on the cmake command line to prevent msi building from 
# updating CPACK_MONOLITHIC_INSTALL in the CMake cache (which will regenerate project files and mess up 
# dependencies). 
# Note that as long as CPACK_MONOLITHIC_INSTALL is set in the top-level CMakeLists.txt, you must 
# manually turn CPACK_MONOLITHIC_INSTALL off in the cache 
# (by running cmake -DCPACK_MONOLITHIC_INSTALL=0 ${CMAKE_BINARY_DIR}) when using 
# WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL=YES. Merely setting CPACK_MONOLITHIC_INSTALL 
# on the command line when running cmake won't work.
# The root cause of this is cmake bug (http://public.kitware.com/Bug/view.php?id=11452) which 
# made it necessary to set CPACK_MONOLITHIC_INSTALL to get correct tars and zips when using the 
# install command with components.  
# The good thing is that the above bug has been fixed in CMake 2.8.4 so maybe these hacks can be 
# removed some day.

IF(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)
	ADD_CUSTOM_TARGET(NDB_MSI
		COMMAND "${CMAKE_COMMAND}" -E copy "${CPACK_PACKAGE_FILE_NAME}.msi" "${CMAKE_BINARY_DIR}/packaging/WiX/${CPACK_PACKAGE_FILE_NAME}.msi"
		DEPENDS "${CPACK_PACKAGE_FILE_NAME}.msi"
		COMMENT "Create msi for NDB"
		VERBATIM)
ELSE(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)
	# Turn component-based install off again.
	# Using a cmake script to do this is cumbersome, but invoking cmake -D... <cache> directly
	# doesn't work. Apparently because the parent cmake process keeps file locks which prevent the child
	# cmake process from executing properly. This does not seem to affect the custom command to turn 
	# component-based install on, maybe because that custom command does not have any dependencies?
	ADD_CUSTOM_TARGET(NDB_MSI
		${CMAKE_SCRIPT_SETUP}
		"-DCACHE_ARGS=-DCPACK_MONOLITHIC_INSTALL=1"
		"-DCACHE_DIR=${CMAKE_BINARY_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/update_cache.cmake"
		COMMAND "${CMAKE_COMMAND}" -E remove mono_off.txt
		COMMAND "${CMAKE_COMMAND}" -E copy "${CPACK_PACKAGE_FILE_NAME}.msi" "${CMAKE_BINARY_DIR}/packaging/WiX/${CPACK_PACKAGE_FILE_NAME}.msi"
		DEPENDS "${CPACK_PACKAGE_FILE_NAME}.msi"
		COMMENT "Run cmake on cache to turn CPACK_MONOLITHIC_INSTALL back on"
		VERBATIM)
ENDIF(WIX_DONT_SET_CPACK_MONOLITHIC_INSTALL)
