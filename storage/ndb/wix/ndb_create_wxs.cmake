# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
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
#
# This is a reimplementation of parts of packaging/WiX/create_msi.cmake to make it
# work with a MySQL Cluster install.

# Directories and files which are part of the install, but which should be 
# ignored when creating the wxs.

# Make it failfast
#  CMP0010 - Bad variable reference syntax is an error.
CMAKE_POLICY(SET CMP0010 NEW)

SET(EXCLUDE_DIRS
     bin/debug
     data/test
     lib/plugin/debug
     mysql-test
     scripts
     sql-bench)

# Files installed by the selected components, but which nevertheless must 
# be exluded from the wxs. Used through get_include() macro below
SET(EXCLUDE_FILES
     bin/echo.exe
     bin/mysqld-debug.exe
     bin/replace.exe
     lib/debug/mysqlserver.lib
     lib/mysqlserver.lib
     lib/mysqlservices.lib
)

# Debug tracing enabled by setting DEBUG_ variable
# Cannot be a macro because of http://public.kitware.com/Bug/view.php?id=5389
SET(DEBUG_ $ENV{WIX_DEBUG})
FUNCTION(DBG s)
	IF(DEBUG_)
		MESSAGE(STATUS "DBG: ${s}")
	ENDIF(DEBUG_)
ENDFUNCTION(DBG)

# Provide rm -f -like functionality
# Note that because of http://public.kitware.com/Bug/view.php?id=5389 ${f} must be 
# a CMake path. A native Windows path will not work.
MACRO(RMF f)
  IF(EXISTS ${f})
    FILE(REMOVE ${f})
  ENDIF()
ENDMACRO(RMF)

# Simplify test for exclusion
# Note that because of http://public.kitware.com/Bug/view.php?id=5389 ${path} must be 
# a CMake path. A native Windows path will not work.
MACRO(GET_INCLUDE var path)
  SET(${var} YES)
  FILE(RELATIVE_PATH rel ${CGRP_ABS} ${path})
  IF(IS_DIRECTORY ${path})
    LIST(FIND EXCLUDE_DIRS ${rel} res)
	IF(NOT res EQUAL -1)
		DBG("gi: excluding directory: ${rel}")
	  SET(${var} NO)
    ENDIF()
  ELSE()
    LIST(FIND EXCLUDE_FILES ${rel} res)
	IF(NOT res EQUAL -1 OR rel MATCHES "\\.pdb")	
	  DBG("gi: excluding file: ${rel}")
	  SET(${var} NO)
	ENDIF()
  ENDIF()
ENDMACRO(GET_INCLUDE)

# Provide ++-like functionality for numeric variables
MACRO(INCR var)
	MATH(EXPR ${var} "${${var}} + 1")
ENDMACRO(INCR)

# Export a local variable to the parent scope
MACRO(TO_PARENT var)
	SET(${var} ${${var}} PARENT_SCOPE)
ENDMACRO(TO_PARENT)

# First argument is variable. Appends all follwing arguments to that variable.
# Cannot be a macro because of http://public.kitware.com/Bug/view.php?id=5389
FUNCTION(SAPPEND_VA var)
	SET(acc ${${var}})
	FOREACH(a ${ARGN})
		SET(acc "${acc}${a}")
	ENDFOREACH()
	SET(${var} ${acc} PARENT_SCOPE)
ENDFUNCTION(SAPPEND_VA)

# FUNCTION WIX_DESCRIBE_DIRS: Traverse empty dummy directory structure to create wxs-xml describing the desired install-layout.
#
# Describing the directory structure is made a bit tricky by the fact that a component-based 
# install, as done by _create_cmake.msi, will (and needs to) create a separate directory for each 
# component. Inside each component directory there is a normal bin, lib, share, ... hierarchy for 
# that component. But when creating the wxs-xml description all the subdirs 
# found in the various component directories must merged into one tree. Describing this merged virtual 
# tree in xml is difficult, e.g. when encountering the share directory for the first component an 
# xml tag for this directory is created. But before the end-tag can be insterted all of the 
# subdirectories of share must be added. But other components may add other directories under share, 
# so no end tags can be inserted until the entire installation has been traversed. In create_msi.cmake
# this was handled by creating the merged directory tree with empty directories while traversing the 
# component-based installation, and then create the wxs-xml for the directories from this tree of 
# empty directories. Since _create_msi.cmake also leaves behind this directory tree it can be used to 
# simplify the logic of the WIX_DESCRIBE_DIRS function.
# Args:
#   dir - directory to describe the members of
#   prefix - indentation space
# Globals:
#   DIR_LIST - list of relative directories encountered. Index in list is the directorys wxs xml id 
#   CPACK_WIX_DIRECTORIES - aggregated wxs xml describing the directory structure
FUNCTION(WIX_DESCRIBE_DIRS dir prefix)
	SET(DIR_LIST ${DIR_LIST}) # Bring into local scope
	SET(CPACK_WIX_DIRECTORIES ${CPACK_WIX_DIRECTORIES})
	FILE(GLOB dlist ${dir}/*)
	FOREACH(d ${dlist})
		FILE(RELATIVE_PATH rpd ${DIRS_TREE} ${d})
		DBG("WIX_DESCRIBE_DIRS: Processing ${rpd}")
		LIST(LENGTH DIR_LIST id)
		LIST(APPEND DIR_LIST ${rpd})
		GET_FILENAME_COMPONENT(name ${d} NAME)	
		SAPPEND_VA(CPACK_WIX_DIRECTORIES "${prefix}<Directory Id='D${id}' Name='${name}'>\n")
		WIX_DESCRIBE_DIRS(${d} "  ${prefix}")
		SAPPEND_VA(CPACK_WIX_DIRECTORIES "${prefix}</Directory> <!-- ${rpd} -->\n")
	ENDFOREACH()
	TO_PARENT(CPACK_WIX_DIRECTORIES)
	TO_PARENT(DIR_LIST)
ENDFUNCTION(WIX_DESCRIBE_DIRS)

# FUNCTION WIX_DESCRIBE_COMPONENTS: Traverse files in each cpack install component directory and 
# create wxs-xml descriptions for each wix-component, (a wix-component is a file or group of files, 
# which is not the same as a cpack install component. The latter is a wix component group).
# 
# This is essentially a re-implementation of the same functionality in create_msi.cmake, 
# but with one crucial difference: Where create_msi.cmake tries to create a 1-to-1 mapping between the 
# (relative) file/directory name and its wxs-xml id, this function uses numeric ids. Trying to convert 
# filenames into ids causes a number of problems:
# 1) Not all characters that are legal in filenames are legal in wxs-xml identifiers
# 2) The old solution of mapping all illegal characters to '_' can (and does for some files in 
#    the Python installation) yield non-unique ids. This is illegal.
# 3) wxs-xml ids can only be 72 characters long. The old solution of 
#    truncating is error-prone as fewer characters are permitted as the first character of an id. 
#    (create_msi.cmake also has bugs where a truncated id is combined with additional character to 
#    produce an id that is still longer than 72 characters. 
# Using numeric ids elliminates these problems but the fact that the id cannot be obtained from the 
# file/directory name creates additional challenges. E.g. the id of a file's parent directory could previously be 
# obtained from the directory portion of the filename. Now this id is not known until the wxs-xml 
# for the directories have been created, and even then the ids are buried in the generated xml. 
# The solution is to store the directorry name (relative to the component directory) in a list, 
# and use the directory's index in this list as the id. The directories must be traversed first 
# to populate the list.

FUNCTION(WIX_DESCRIBE_COMPONENTS dir prefix)
	SET(CPACK_WIX_COMPONENT_GROUPS ${CPACK_WIX_COMPONENT_GROUPS}) # Bring into local scope
	FILE(RELATIVE_PATH rpdname ${CGRP_ABS} ${dir})
	IF(NOT rpdname)
		SET(rpdname INSTALLDIR)
	ENDIF()

	SET(exe_list)
	SET(non_dir_list)
	FILE(GLOB all_files ${dir}/*)
	FOREACH(f ${all_files})
		GET_INCLUDE(inc ${f})
		IF(inc)
			IF(IS_DIRECTORY ${f})
				WIX_DESCRIBE_COMPONENTS(${f} "  ${prefix}")
			ELSE()
				GET_FILENAME_COMPONENT(f_ext "${f}" EXT)
				IF(f_ext MATCHES ".exe" OR f_ext MATCHES ".dll")
					LIST(APPEND exe_list ${f})
				ELSE()
					LIST(APPEND non_dir_list ${f})
				ENDIF()
			ENDIF()
		ENDIF(inc)
	ENDFOREACH()
  
	IF(non_dir_list OR exe_list)
		LIST(FIND DIR_LIST ${rpdname} dix)
		IF(dix EQUAL -1)
			MESSAGE(FATAL_ERROR "Unable to locate ${rpdname} (${dir}) in DIR_LIST")
		ENDIF()
		IF(dix EQUAL 0)
			SET(d_id INSTALLDIR)
		ELSE()
			SET(d_id "D${dix}")
	    ENDIF()
		SAPPEND_VA(CPACK_WIX_COMPONENTS "${prefix}<DirectoryRef Id='${d_id}'>\n")
  
		FOREACH(exe ${exe_list})
			INCR(id)
			SET(cid "${CGRP_NAME}.F${id}")
			FILE(RELATIVE_PATH rpexename ${CGRP_ABS} ${exe})
			DBG("F${id}: ${rpexename}")
			FILE(TO_NATIVE_PATH ${exe} exe_native)
			SAPPEND_VA(CPACK_WIX_COMPONENT_GROUPS "      <ComponentRef Id='${cid}'/>\n")			
			SAPPEND_VA(CPACK_WIX_COMPONENTS 
				"${prefix}  <Component Id='${cid}' Guid='*' ${Win64}>\n"
				"${prefix}    <File Id='F${id}' KeyPath='yes' Source='${exe_native}'/>\n"
				"${prefix}  </Component>\n")
		ENDFOREACH()
  
		IF(non_dir_list)
			EXECUTE_PROCESS(COMMAND uuidgen -c 
				OUTPUT_VARIABLE guid
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			SET(cid "${CGRP_NAME}.${d_id}.files")				
			SAPPEND_VA(CPACK_WIX_COMPONENTS 
				"${prefix}  <Component Guid='${guid}' Id='${cid}' ${Win64}>\n")
			FOREACH(non_dir ${non_dir_list})
				INCR(id)
				FILE(RELATIVE_PATH rpnondirname ${CGRP_ABS} ${non_dir})
				FILE(TO_NATIVE_PATH ${non_dir} non_dir_native)
				DBG("F${id}: ${rpnondirname}")
				SAPPEND_VA(CPACK_WIX_COMPONENTS 
					"${prefix}    <File Id='F${id}' Source='${non_dir_native}'/>\n")
			ENDFOREACH()
			SAPPEND_VA(CPACK_WIX_COMPONENTS "${prefix}  </Component>\n")
			SAPPEND_VA(CPACK_WIX_COMPONENT_GROUPS "      <ComponentRef Id='${cid}'/>\n")
		ENDIF(non_dir_list)	
		SAPPEND_VA(CPACK_WIX_COMPONENTS "${prefix}</DirectoryRef> <!-- ${rpdname} -->\n")
	ENDIF(non_dir_list OR exe_list)
	
	TO_PARENT(id)
	TO_PARENT(CPACK_WIX_COMPONENT_GROUPS)
	TO_PARENT(CPACK_WIX_COMPONENTS)
ENDFUNCTION(WIX_DESCRIBE_COMPONENTS)

# Main script

# Describe directories by traversing empty directory tree left by _create_msi.cmake. 
# Populate DIR_LIST with directory names relatvie to component group directory
SET(CPACK_WIX_DIRECTORIES "<DirectoryRef Id='INSTALLDIR'>\n")	
SET(DIR_LIST INSTALLDIR)
GET_FILENAME_COMPONENT(DIRS_TREE ./dirs ABSOLUTE)

DBG("DIRS_TREE: ${DIRS_TREE}")

WIX_DESCRIBE_DIRS(${DIRS_TREE} "      ")
IF(NOT DIR_LIST)
	MESSAGE(FATAL_ERROR "DIR_LIST is empty!")
ENDIF()
SAPPEND_VA(CPACK_WIX_DIRECTORIES "    </DirectoryRef> <!-- INSTALLDIR -->\n")

# Describe the files of the installation. Top-level directories are the component groups 
# (which confusingly enough correspond to the COMPONENT argument of the cmake INSTALL command), so 
# WIX_DESCRIBE_COMPONENTS() is called for each group
SET(id 0)
SET(CPACK_WIX_COMPONENTS "\n")       # To get indentation right
SET(CPACK_WIX_COMPONENT_GROUPS "\n") # To get indentation right
FILE(GLOB COMPGRP_LIST testinstall/*)
FOREACH(cgrp ${COMPGRP_LIST})
	GET_FILENAME_COMPONENT(CGRP_NAME ${cgrp} NAME)
	GET_FILENAME_COMPONENT(CGRP_ABS ${cgrp} ABSOLUTE)
	SAPPEND_VA(CPACK_WIX_COMPONENT_GROUPS "    <ComponentGroup Id='componentgroup.${CGRP_NAME}'>\n")
	WIX_DESCRIBE_COMPONENTS(${CGRP_ABS} "    ")	
	SAPPEND_VA(CPACK_WIX_COMPONENT_GROUPS "    </ComponentGroup>\n")
ENDFOREACH()

# Expand the remaining variables in the intermediate wxs.in to create the wxs
CONFIGURE_FILE("${WXS_BASENAME}.wxs.in" "${WXS_BASENAME}.wxs" @ONLY)
