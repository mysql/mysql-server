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

GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)
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
    IF(CMAKE_GENERATOR MATCHES "Visual Studio")
      STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CMAKE_INSTALL_CONFIG_NAME}" pdb_location ${pdb_location})
    ENDIF()
    IF(target STREQUAL "mysqld")
	  SET(comp Server)
    ELSE()
      SET(comp Debuginfo)
    ENDIF()	  
    INSTALL(FILES ${pdb_location} DESTINATION ${INSTALL_LOCATION} COMPONENT ${comp})
  ENDFOREACH()
  ENDIF()
ENDMACRO()

# Installs manpage for given file (either script or executable)
# 
FUNCTION(INSTALL_MANPAGE file)
  IF(NOT UNIX)
    RETURN()
  ENDIF()
  GET_FILENAME_COMPONENT(file_name "${file}" NAME)
  SET(GLOB_EXPR 
    ${CMAKE_SOURCE_DIR}/man/*${file}man.1*
    ${CMAKE_SOURCE_DIR}/man/*${file}man.8*
    ${CMAKE_BINARY_DIR}/man/*${file}man.1*
    ${CMAKE_BINARY_DIR}/man/*${file}man.8*
   )
  IF(MYSQL_DOC_DIR)
    SET(GLOB_EXPR 
      ${MYSQL_DOC_DIR}/man/*${file}man.1*
      ${MYSQL_DOC_DIR}/man/*${file}man.8*
      ${MYSQL_DOC_DIR}/man/*${file}.1*
      ${MYSQL_DOC_DIR}/man/*${file}.8*
      ${GLOB_EXPR}
      )
   ENDIF()
    
  FILE(GLOB_RECURSE MANPAGES ${GLOB_EXPR})
  IF(MANPAGES)
    LIST(GET MANPAGES 0 MANPAGE)
    STRING(REPLACE "${file}man.1" "${file}.1" MANPAGE "${MANPAGE}")
    STRING(REPLACE "${file}man.8" "${file}.8" MANPAGE "${MANPAGE}")
    IF(MANPAGE MATCHES "${file}.1")
      SET(SECTION man1)
    ELSE()
      SET(SECTION man8)
    ENDIF()
    INSTALL(FILES "${MANPAGE}" DESTINATION "${INSTALL_MANDIR}/${SECTION}")
  ENDIF()
ENDFUNCTION()

FUNCTION(INSTALL_SCRIPT)
 CMAKE_PARSE_ARGUMENTS(ARG
  "DESTINATION;COMPONENT"
  ""
  ${ARGN}
  )
  
  SET(script ${ARG_DEFAULT_ARGS})
  IF(NOT ARG_DESTINATION)
    SET(ARG_DESTINATION ${INSTALL_BINDIR})
  ENDIF()
  IF(ARG_COMPONENT)
    SET(COMP COMPONENT ${ARG_COMPONENT})
  ELSE()
    SET(COMP)
  ENDIF()

  INSTALL(FILES 
    ${script}
    DESTINATION ${ARG_DESTINATION}
    PERMISSIONS OWNER_READ OWNER_WRITE 
    OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE  ${COMP}
  )
  INSTALL_MANPAGE(${script})
ENDFUNCTION()

# Install symbolic link to CMake target. 
# the link is created in the same directory as target
# and extension will be the same as for target file.
MACRO(INSTALL_SYMLINK linkbasename target destination component)
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
  IF(CMAKE_GENERATOR MATCHES "Xcode")
    # For Xcode, replace project config with install config
    STRING(REPLACE "${CMAKE_CFG_INTDIR}" 
      "\${CMAKE_INSTALL_CONFIG_NAME}" output ${output})
  ENDIF()
  IF(component)
    SET(COMP COMPONENT ${component})
  ELSE()  
    SET(COMP)
  ENDIF()
  INSTALL(FILES ${output} DESTINATION ${destination} ${COMP})
ENDIF()
ENDMACRO()

IF(WIN32)
  OPTION(SIGNCODE "Sign executables and dlls with digital certificate" OFF)
  MARK_AS_ADVANCED(SIGNCODE)
  IF(SIGNCODE)
   SET(SIGNTOOL_PARAMETERS 
     /a /t http://timestamp.verisign.com/scripts/timstamp.dll
     CACHE STRING "parameters for signtool (list)")
    FIND_PROGRAM(SIGNTOOL_EXECUTABLE signtool)
    IF(NOT SIGNTOOL_EXECUTABLE)
      MESSAGE(FATAL_ERROR 
      "signtool is not found. Signing executables not possible")
    ENDIF()
    IF(NOT DEFINED SIGNCODE_ENABLED)
      FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/testsign.c "int main(){return 0;}")
      MAKE_DIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/testsign)
     TRY_COMPILE(RESULT ${CMAKE_CURRENT_BINARY_DIR}/testsign ${CMAKE_CURRENT_BINARY_DIR}/testsign.c  
      COPY_FILE ${CMAKE_CURRENT_BINARY_DIR}/testsign.exe
     )
      
     EXECUTE_PROCESS(COMMAND 
      ${SIGNTOOL_EXECUTABLE} sign ${SIGNTOOL_PARAMETERS} ${CMAKE_CURRENT_BINARY_DIR}/testsign.exe
      RESULT_VARIABLE ERR ERROR_QUIET OUTPUT_QUIET
      )
      IF(ERR EQUAL 0)
       SET(SIGNCODE_ENABLED 1 CACHE INTERNAL "Can sign executables")
      ELSE()
       MESSAGE(STATUS "Disable authenticode signing for executables")
        SET(SIGNCODE_ENABLED 0 CACHE INTERNAL "Invalid or missing certificate")
      ENDIF()
    ENDIF()
    MARK_AS_ADVANCED(SIGNTOOL_EXECUTABLE  SIGNTOOL_PARAMETERS)
  ENDIF()
ENDIF()

MACRO(SIGN_TARGET target)
 GET_TARGET_PROPERTY(target_type ${target} TYPE)
 IF(target_type AND NOT target_type MATCHES "STATIC")
   GET_TARGET_PROPERTY(target_location ${target}  LOCATION)
   IF(CMAKE_GENERATOR MATCHES "Visual Studio")
   STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CMAKE_INSTALL_CONFIG_NAME}" 
     target_location ${target_location})
   ENDIF()
   INSTALL(CODE
   "EXECUTE_PROCESS(COMMAND 
     ${SIGNTOOL_EXECUTABLE} sign ${SIGNTOOL_PARAMETERS} ${target_location}
     RESULT_VARIABLE ERR)
    IF(NOT \${ERR} EQUAL 0)
      MESSAGE(FATAL_ERROR \"Error signing  ${target_location}\")
    ENDIF()
   ")
 ENDIF()
ENDMACRO()


# Installs targets, also installs pdbs on Windows.
#
#

FUNCTION(MYSQL_INSTALL_TARGETS)
  CMAKE_PARSE_ARGUMENTS(ARG
    "DESTINATION;COMPONENT"
  ""
  ${ARGN}
  )
  SET(TARGETS ${ARG_DEFAULT_ARGS})
  IF(NOT TARGETS)
    MESSAGE(FATAL_ERROR "Need target list for MYSQL_INSTALL_TARGETS")
  ENDIF()
  IF(NOT ARG_DESTINATION)
     MESSAGE(FATAL_ERROR "Need DESTINATION parameter for MYSQL_INSTALL_TARGETS")
  ENDIF()

 
  FOREACH(target ${TARGETS})
    # If signing is required, sign executables before installing
     IF(SIGNCODE AND SIGNCODE_ENABLED)
      SIGN_TARGET(${target})
    ENDIF()
    # For Windows, add version info to executables
    ADD_VERSION_INFO(${target})
    # Install man pages on Unix
    IF(UNIX)
      GET_TARGET_PROPERTY(target_location ${target} LOCATION)
      INSTALL_MANPAGE(${target_location})
    ENDIF()
  ENDFOREACH()
  IF(ARG_COMPONENT)
    SET(COMP COMPONENT ${ARG_COMPONENT})
  ENDIF()
  INSTALL(TARGETS ${TARGETS} DESTINATION ${ARG_DESTINATION} ${COMP})
  SET(INSTALL_LOCATION ${ARG_DESTINATION} )
  INSTALL_DEBUG_SYMBOLS("${TARGETS}")
  SET(INSTALL_LOCATION)
ENDFUNCTION()

