# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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

FUNCTION (INSTALL_DEBUG_SYMBOLS)
 IF(MSVC)
 MYSQL_PARSE_ARGUMENTS(ARG
  "COMPONENT;INSTALL_LOCATION"
  ""
  ${ARGN}
  )
  
  IF(NOT ARG_COMPONENT)
    SET(ARG_COMPONENT DebugBinaries)
  ENDIF()
  IF(NOT ARG_INSTALL_LOCATION)
    SET(ARG_INSTALL_LOCATION lib)
  ENDIF()
  SET(targets ${ARG_DEFAULT_ARGS})
  FOREACH(target ${targets})
    GET_TARGET_PROPERTY(type ${target} TYPE)
    GET_TARGET_PROPERTY(location ${target} LOCATION)
    STRING(REPLACE ".exe" ".pdb" pdb_location ${location})
    STRING(REPLACE ".dll" ".pdb" pdb_location ${pdb_location})
    STRING(REPLACE ".lib" ".pdb" pdb_location ${pdb_location})
    IF(CMAKE_GENERATOR MATCHES "Visual Studio")
      STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CMAKE_INSTALL_CONFIG_NAME}" pdb_location ${pdb_location})
    ENDIF()
	
    set(comp "")
    IF(ARG_COMPONENT STREQUAL "Server")
      IF(target MATCHES "mysqld" OR type MATCHES "MODULE")
        #MESSAGE("PDB: ${targets}")
        SET(comp Server)
      ENDIF()
    ENDIF()
 
    IF(NOT comp MATCHES Server)
      IF(ARG_COMPONENT MATCHES Development
        OR ARG_COMPONENT MATCHES SharedLibraries
        OR ARG_COMPONENT MATCHES Embedded)
        SET(comp Debuginfo)
      ENDIF()
    ENDIF()

    IF(NOT comp)
      SET(comp Debuginfo_archive_only) # not in MSI
    ENDIF()
	IF(type MATCHES "STATIC")
	  # PDB for static libraries might be unsupported http://public.kitware.com/Bug/view.php?id=14600
	  SET(opt OPTIONAL)
	ENDIF()
    INSTALL(FILES ${pdb_location} DESTINATION ${ARG_INSTALL_LOCATION} COMPONENT ${comp} ${opt})
  ENDFOREACH()
  ENDIF()
ENDFUNCTION()

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
    INSTALL(FILES "${MANPAGE}" DESTINATION "${INSTALL_MANDIR}/${SECTION}"
      COMPONENT ManPages)
  ENDIF()
ENDFUNCTION()

FUNCTION(INSTALL_SCRIPT)
 MYSQL_PARSE_ARGUMENTS(ARG
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


FUNCTION(INSTALL_DOCUMENTATION)
  MYSQL_PARSE_ARGUMENTS(ARG "COMPONENT" "" ${ARGN})
  SET(files ${ARG_DEFAULT_ARGS})
  IF(NOT ARG_COMPONENT)
    SET(ARG_COMPONENT Server)
  ENDIF()
  IF (ARG_COMPONENT MATCHES "Readme")
    SET(destination ${INSTALL_DOCREADMEDIR})
  ELSE()
    SET(destination ${INSTALL_DOCDIR})
  ENDIF()

  STRING(TOUPPER ${ARG_COMPONENT} COMPUP)
  IF(CPACK_COMPONENT_${COMPUP}_GROUP)
    SET(group ${CPACK_COMPONENT_${COMPUP}_GROUP})
  ELSE()
    SET(group ${ARG_COMPONENT})
  ENDIF()

  IF(RPM)
    SET(destination "${destination}/MariaDB-${group}-${VERSION}")
  ELSEIF(DEB)
    SET(destination "${destination}/mariadb-${group}-${MAJOR_VERSION}.${MINOR_VERSION}")
  ENDIF()

  INSTALL(FILES ${files} DESTINATION ${destination} COMPONENT ${ARG_COMPONENT})
ENDFUNCTION()


# Install symbolic link to CMake target. 
# the link is created in the same directory as target
# and extension will be the same as for target file.
MACRO(INSTALL_SYMLINK linkname target destination component)
IF(UNIX)
  GET_TARGET_PROPERTY(location ${target} LOCATION)
  GET_FILENAME_COMPONENT(path ${location} PATH)
  GET_FILENAME_COMPONENT(name ${location} NAME)
  SET(output ${path}/${linkname})
  ADD_CUSTOM_COMMAND(
    OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} ARGS -E remove -f ${output}
    COMMAND ${CMAKE_COMMAND} ARGS -E create_symlink 
      ${name} 
      ${linkname}
    WORKING_DIRECTORY ${path}
    DEPENDS ${target}
    )
  
  ADD_CUSTOM_TARGET(symlink_${linkname}
    ALL
    DEPENDS ${output})
  SET_TARGET_PROPERTIES(symlink_${linkname} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  IF(CMAKE_GENERATOR MATCHES "Xcode")
    # For Xcode, replace project config with install config
    STRING(REPLACE "${CMAKE_CFG_INTDIR}" 
      "\${CMAKE_INSTALL_CONFIG_NAME}" output ${output})
  ENDIF()
  INSTALL(FILES ${output} DESTINATION ${destination} COMPONENT ${component})
ENDIF()
ENDMACRO()

IF(WIN32)
  OPTION(SIGNCODE "Sign executables and dlls with digital certificate" OFF)
  MARK_AS_ADVANCED(SIGNCODE)
  IF(SIGNCODE)
   SET(SIGNTOOL_PARAMETERS 
     /a /t http://timestamp.verisign.com/scripts/timstamp.dll
     CACHE STRING "parameters for signtool (list)")
    FIND_PROGRAM(SIGNTOOL_EXECUTABLE signtool 
      PATHS "$ENV{ProgramFiles}/Microsoft SDKs/Windows/v7.0A/bin"
      "$ENV{ProgramFiles}/Windows Kits/8.0/bin/x86"
    )
    IF(NOT SIGNTOOL_EXECUTABLE)
      MESSAGE(FATAL_ERROR 
      "signtool is not found. Signing executables not possible")
    ENDIF()
    MARK_AS_ADVANCED(SIGNTOOL_EXECUTABLE  SIGNTOOL_PARAMETERS)
  ENDIF()
ENDIF()

MACRO(SIGN_TARGET)
 MYSQL_PARSE_ARGUMENTS(ARG "COMPONENT" "" ${ARGN})
 SET(target ${ARG_DEFAULT_ARGS})
 IF(ARG_COMPONENT)
  SET(comp COMPONENT ${ARG_COMPONENT})
 ELSE()
  SET(comp)
 ENDIF()
 GET_TARGET_PROPERTY(target_type ${target} TYPE)
 IF(target_type AND NOT target_type MATCHES "STATIC")
   GET_TARGET_PROPERTY(target_location ${target}  LOCATION)
   IF(CMAKE_GENERATOR MATCHES "Visual Studio")
   STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CMAKE_INSTALL_CONFIG_NAME}" 
     target_location ${target_location})
   ENDIF()
   INSTALL(CODE
   "EXECUTE_PROCESS(COMMAND 
   \"${SIGNTOOL_EXECUTABLE}\" verify /pa /q \"${target_location}\"
   RESULT_VARIABLE ERR)
   IF(NOT \${ERR} EQUAL 0)
     EXECUTE_PROCESS(COMMAND 
     \"${SIGNTOOL_EXECUTABLE}\" sign ${SIGNTOOL_PARAMETERS} \"${target_location}\"
     RESULT_VARIABLE ERR)
   ENDIF()
   IF(NOT \${ERR} EQUAL 0)
    MESSAGE(FATAL_ERROR \"Error signing  '${target_location}'\")
   ENDIF()
   " ${comp})
 ENDIF()
ENDMACRO()


# Installs targets, also installs pdbs on Windows.
#
#

FUNCTION(MYSQL_INSTALL_TARGETS)
  MYSQL_PARSE_ARGUMENTS(ARG
    "DESTINATION;COMPONENT"
  ""
  ${ARGN}
  )
  IF(ARG_COMPONENT)
    SET(COMP COMPONENT ${ARG_COMPONENT})
  ELSE()
    MESSAGE(FATAL_ERROR "COMPONENT argument required")
  ENDIF()
  
  SET(TARGETS ${ARG_DEFAULT_ARGS})
  IF(NOT TARGETS)
    MESSAGE(FATAL_ERROR "Need target list for MYSQL_INSTALL_TARGETS")
  ENDIF()
  IF(NOT ARG_DESTINATION)
     MESSAGE(FATAL_ERROR "Need DESTINATION parameter for MYSQL_INSTALL_TARGETS")
  ENDIF()

 
  FOREACH(target ${TARGETS})
    # If signing is required, sign executables before installing
     IF(SIGNCODE)
      SIGN_TARGET(${target} ${COMP})
    ENDIF()
    # Install man pages on Unix
    IF(UNIX)
      GET_TARGET_PROPERTY(target_location ${target} LOCATION)
      INSTALL_MANPAGE(${target_location})
    ENDIF()
  ENDFOREACH()

  INSTALL(TARGETS ${TARGETS} DESTINATION ${ARG_DESTINATION} ${COMP})
  INSTALL_DEBUG_SYMBOLS(${TARGETS} ${COMP} INSTALL_LOCATION ${ARG_DESTINATION})

ENDFUNCTION()

# Optionally install mysqld/client/embedded from debug build run. outside of the current build dir 
# (unless multi-config generator is used like Visual Studio or Xcode). 
# For Makefile generators we default Debug build directory to ${buildroot}/../debug.
GET_FILENAME_COMPONENT(BINARY_PARENTDIR ${CMAKE_BINARY_DIR} PATH)
SET(DEBUGBUILDDIR "${BINARY_PARENTDIR}/debug" CACHE INTERNAL "Directory of debug build")


FUNCTION(INSTALL_DEBUG_TARGET target)
 MYSQL_PARSE_ARGUMENTS(ARG
  "DESTINATION;RENAME;PDB_DESTINATION;COMPONENT"
  ""
  ${ARGN}
  )
  GET_TARGET_PROPERTY(target_type ${target} TYPE)
  IF(ARG_RENAME)
    SET(RENAME_PARAM RENAME ${ARG_RENAME}${CMAKE_${target_type}_SUFFIX})
  ELSE()
    SET(RENAME_PARAM)
  ENDIF()
  IF(NOT ARG_DESTINATION)
    MESSAGE(FATAL_ERROR "Need DESTINATION parameter for INSTALL_DEBUG_TARGET")
  ENDIF()
  GET_TARGET_PROPERTY(target_location ${target} LOCATION)
  IF(CMAKE_GENERATOR MATCHES "Makefiles")
   STRING(REPLACE "${CMAKE_BINARY_DIR}" "${DEBUGBUILDDIR}"  debug_target_location "${target_location}")
  ELSE()
   STRING(REPLACE "${CMAKE_CFG_INTDIR}" "Debug"  debug_target_location "${target_location}" )
  ENDIF()
  IF(NOT ARG_COMPONENT)
    SET(ARG_COMPONENT DebugBinaries)
  ENDIF()
  
  # Define permissions
  # For executable files
  SET(PERMISSIONS_EXECUTABLE
      PERMISSIONS
      OWNER_READ OWNER_WRITE OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE)

  # Permissions for shared library (honors CMAKE_INSTALL_NO_EXE which is 
  # typically set on Debian)
  IF(CMAKE_INSTALL_SO_NO_EXE)
    SET(PERMISSIONS_SHARED_LIBRARY
      PERMISSIONS
      OWNER_READ OWNER_WRITE 
      GROUP_READ
      WORLD_READ)
  ELSE()
    SET(PERMISSIONS_SHARED_LIBRARY ${PERMISSIONS_EXECUTABLE})
  ENDIF()

  # Shared modules get the same permissions as shared libraries
  SET(PERMISSIONS_MODULE_LIBRARY ${PERMISSIONS_SHARED_LIBRARY})

  #  Define permissions for static library
  SET(PERMISSIONS_STATIC_LIBRARY
      PERMISSIONS
      OWNER_READ OWNER_WRITE 
      GROUP_READ
      WORLD_READ)

  INSTALL(FILES ${debug_target_location}
    DESTINATION ${ARG_DESTINATION}
    ${RENAME_PARAM}
    ${PERMISSIONS_${target_type}}
    CONFIGURATIONS Release RelWithDebInfo
    COMPONENT ${ARG_COMPONENT}
    OPTIONAL)

  IF(MSVC)
    GET_FILENAME_COMPONENT(ext ${debug_target_location} EXT)
    STRING(REPLACE "${ext}" ".pdb"  debug_pdb_target_location "${debug_target_location}" )
    IF (RENAME_PARAM)
      IF(NOT ARG_PDB_DESTINATION)
        STRING(REPLACE "${ext}" ".pdb"  "${ARG_RENAME}" pdb_rename)
        SET(PDB_RENAME_PARAM RENAME "${pdb_rename}")
      ENDIF()
    ENDIF()
    IF(NOT ARG_PDB_DESTINATION)
      SET(ARG_PDB_DESTINATION "${ARG_DESTINATION}")
    ENDIF()
    INSTALL(FILES ${debug_pdb_target_location}
      DESTINATION ${ARG_PDB_DESTINATION}
      ${PDB_RENAME_PARAM}
      CONFIGURATIONS Release RelWithDebInfo
      COMPONENT ${ARG_COMPONENT}
      OPTIONAL)
  ENDIF()
ENDFUNCTION()


FUNCTION(INSTALL_MYSQL_TEST from to)
  IF(INSTALL_MYSQLTESTDIR)
    INSTALL(
      DIRECTORY ${from}
      DESTINATION "${INSTALL_MYSQLTESTDIR}/${to}"
      USE_SOURCE_PERMISSIONS
      COMPONENT Test
      PATTERN "var" EXCLUDE
      PATTERN "lib/My/SafeProcess" EXCLUDE
      PATTERN "lib/t*" EXCLUDE
      PATTERN "CPack" EXCLUDE
      PATTERN "CMake*" EXCLUDE
      PATTERN "cmake_install.cmake" EXCLUDE
      PATTERN "mtr.out*" EXCLUDE
      PATTERN ".cvsignore" EXCLUDE
      PATTERN "*.am" EXCLUDE
      PATTERN "*.in" EXCLUDE
      PATTERN "Makefile" EXCLUDE
      PATTERN "*.vcxproj" EXCLUDE
      PATTERN "*.vcxproj.filters" EXCLUDE
      PATTERN "*.vcxproj.user" EXCLUDE
      PATTERN "CTest*" EXCLUDE
      PATTERN "*~" EXCLUDE
    )
  ENDIF()
ENDFUNCTION()
