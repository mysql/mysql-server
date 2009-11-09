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

# Merge static libraries.
MACRO(MERGE_STATIC_LIBS TARGET OUTPUT_NAME LIBS_TO_MERGE)
  # To produce a library we need at least one source file.
  # It is created by ADD_CUSTOM_COMMAND below and will helps 
  # also help to track dependencies.
  SET(SOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_depends.c)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCE_FILE})
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

  FOREACH(LIB ${LIBS_TO_MERGE})
    GET_TARGET_PROPERTY(LIB_LOCATION ${LIB} LOCATION)
    GET_TARGET_PROPERTY(LIB_TYPE ${LIB} TYPE)
    IF(NOT LIB_LOCATION)
       # 3rd party library like libz.so. Make sure that everything
       # that links to our library links to this one as well.
       TARGET_LINK_LIBRARIES(${TARGET} ${LIB})
    ELSE()
      # This is a target in current project
      # (can be a static or shared lib)
      IF(LIB_TYPE STREQUAL "STATIC_LIBRARY")
        SET(STATIC_LIBS ${STATIC_LIBS} ${LIB_LOCATION})
        ADD_DEPENDENCIES(${TARGET} ${LIB})
      ELSE()
        # This is a shared library our static lib depends on.
        TARGET_LINK_LIBRARIES(${TARGET} ${LIB})
      ENDIF()
    ENDIF()
  ENDFOREACH()

  # Make the generated dummy source file depended on all static input
  # libs. If input lib changes,the source file is touched
  # which causes the desired effect (relink).
  ADD_CUSTOM_COMMAND( 
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND}  -E touch ${SOURCE_FILE}
    DEPENDS ${STATIC_LIBS})

  IF(MSVC)
    # To merge libs, just pass them to lib.exe command line.
    SET(LINKER_EXTRA_FLAGS "")
    FOREACH(LIB ${STATIC_LIBS})
      SET(LINKER_EXTRA_FLAGS "${LINKER_EXTRA_FLAGS} ${LIB}")
    ENDFOREACH()
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES STATIC_LIBRARY_FLAGS 
      "${LINKER_EXTRA_FLAGS}")
  ELSE()
    GET_TARGET_PROPERTY(TARGET_LOCATION ${TARGET} LOCATION)  
    IF(APPLE)
      # Use OSX's libtool to merge archives (ihandles universal 
	  # binaries properly)
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND rm ${TARGET_LOCATION}
        COMMAND /usr/bin/libtool -static -o ${TARGET_LOCATION} 
		  ${STATIC_LIBS}
      )  
    ELSE()
	  # Generic Unix, Cygwin or MinGW. In post-build step, call
      # script, that extracts objects from archives with "ar x" 
	  # and repacks them with "ar r"
      SET(TARGET ${TARGET})
      CONFIGURE_FILE(
        ${CMAKE_SOURCE_DIR}/cmake/merge_archives_unix.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake 
        @ONLY
      )
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
	    COMMAND rm ${TARGET_LOCATION}
        COMMAND ${CMAKE_COMMAND} -P 
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake
      )
    ENDIF()
  ENDIF()
ENDMACRO()

# Convert static library to shared
MACRO(STATIC_TO_SHARED STATIC_LIB SHARED_LIB EXPORTS_FILE)
  IF(NOT MSVC)
   MESSAGE(FATAL_ERROR 
   "Cannot convert static ${STATIC_LIB} to shared ${TARGET} library."
   )
  ENDIF()
  
  # Need one source file.
  SET(SOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIB}_dummy.c)
  ADD_CUSTOM_COMMAND( 
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${SOURCE_FILE}
  )

  ADD_LIBRARY(${SHARED_LIB} SHARED ${SOURCE_FILE} ${EXPORTS_FILE})
  TARGET_LINK_LIBRARIES(${SHARED_LIB} ${STATIC_LIB})
ENDMACRO()

MACRO(SET_TARGET_SOURCEDIR TARGET)
  SET(${TARGET}_SOURCEDIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "source directory for a target")
ENDMACRO()

# Handy macro to use when source projects maybe used somewhere else
# For example, embedded or client library may recompile mysys sources
# In such cases, using absolute names in ADD_LIBRARY has the advantage that 
# GET_TARGET_PROPERTY(xxx SOURCES) also returns absolute names, so there is
# no need to know the base directory of a target.
MACRO(USE_ABSOLUTE_FILENAMES FILELIST)
  # Use absolute file paths for sources
  # It helps when building embedded where we need to 
  # sources  files for the plugin to recompile.
  SET(RESOLVED_PATHS)
  FOREACH(FILE  ${${FILELIST}})
    GET_FILENAME_COMPONENT(ABSOLUTE_PATH ${FILE} ABSOLUTE)
	LIST(APPEND RESOLVED_PATHS ${ABSOLUTE_PATH})
  ENDFOREACH()
  SET(${FILELIST} ${RESOLVED_PATHS})
ENDMACRO()
