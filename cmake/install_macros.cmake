# Copyright (C) 2009 Sun Microsystems, Inc
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

MACRO (INSTALL_DEBUG_SYMBOLS targets)
  IF(MSVC)
  FOREACH(target ${targets})
    GET_TARGET_PROPERTY(location ${target} LOCATION)
    GET_TARGET_PROPERTY(type ${target} TYPE)
    IF(NOT INSTALL_LOCATION)
	  IF(type MATCHES "STATIC_LIBRARY" OR type MATCHES "MODULE_LIBRARY" OR type MATCHES "SHARED_LIBRARY")
        SET(INSTALL_LOCATION "lib")
	  ELSEIF(type MATCHES "EXECUTABLE")
        SET(INSTALL_LOCATION "bin")
	  ELSE()
        MESSAGE(FATAL_ERROR "cannot determine type of ${target}. Don't now where to install")
	  ENDIF()
    ENDIF()
    STRING(REPLACE ".exe" ".pdb" pdb_location ${location})
    STRING(REPLACE ".dll" ".pdb" pdb_location ${pdb_location})
    STRING(REPLACE ".lib" ".pdb" pdb_location ${pdb_location})
    STRING(REPLACE "$(OutDir)" "\${CMAKE_INSTALL_CONFIG_NAME}" pdb_location ${pdb_location})
    STRING(REPLACE "$(ConfigurationName)" "\${CMAKE_INSTALL_CONFIG_NAME}" pdb_location ${pdb_location})
    INSTALL(FILES ${pdb_location} DESTINATION ${INSTALL_LOCATION})
  ENDFOREACH()
  ENDIF()
ENDMACRO()

# Install symbolic link to CMake target. 
# the link is created in the same directory as target
# and extension will be the same as for target file.
MACRO(INSTALL_SYMLINK linkbasename target destination)
IF(UNIX)
  GET_TARGET_PROPERTY(location ${target} LOCATION)
  GET_FILENAME_COMPONENT(path ${location} PATH)
  GET_FILENAME_COMPONENT(name_we ${location} NAME_WE)
  GET_FILENAME_COMPONENT(ext ${location} EXT)
  SET(output ${path}/${linkbasename}${ext})
  ADD_CUSTOM_COMMAND(
    OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} ARGS -E remove -f ${output}
    COMMAND ${CMAKE_COMMAND} ARGS -E create_symlink 
      ${name_we}${ext} 
      ${linkbasename}${ext}
    WORKING_DIRECTORY ${path}
    DEPENDS ${target}
    )
  
  ADD_CUSTOM_TARGET(symlink_${linkbasename}${ext}
    ALL
    DEPENDS ${output})
  SET_TARGET_PROPERTIES(symlink_${linkbasename}${ext} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  # For Xcode, replace project config with install config
  STRING(REPLACE "$(CONFIGURATION)" "\${CMAKE_INSTALL_CONFIG_NAME}" output ${output})
  INSTALL(FILES ${output} DESTINATION ${destination})
ENDIF()
ENDMACRO()

