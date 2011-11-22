# Copyright (c) 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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

# Generates an ELF object file with dtrace entry points.
# This object that must to be linked with together with 
# the target. This script needs to run on Solaris only

# Do not follow symlinks in GLOB_RECURSE
CMAKE_POLICY(SET CMP0009 NEW)
FILE(REMOVE ${OUTFILE})

MACRO(CONVERT_TO_RELATIVE_PATHS files rel_paths)
  GET_FILENAME_COMPONENT(abs_dir . ABSOLUTE)
  SET(${rel_paths})
  FOREACH(file ${files})
    FILE(RELATIVE_PATH rel  ${abs_dir} ${file})
    LIST(APPEND ${rel_paths} ${rel})
  ENDFOREACH()
ENDMACRO()

IF(TYPE STREQUAL "MERGE")
  # Rerun dtrace on objects that are already in static libraries.
  # Object paths are stored in text files named 'dtrace_objects'
  # in the input directores. We have to copy the objects into temp.
  # directory, as running dtrace -G on original files will change
  # timestamps and cause rebuilds or the libraries / excessive 
  # relink
  FILE(REMOVE_RECURSE dtrace_objects_merge)
  MAKE_DIRECTORY(dtrace_objects_merge)

  FOREACH(dir ${DIRS})
    FILE(STRINGS ${dir}/dtrace_objects  OBJS)
    FOREACH(obj ${OBJS})
      IF(obj)
        EXECUTE_PROCESS(COMMAND cp ${obj} dtrace_objects_merge)
      ENDIF()
    ENDFOREACH()
  ENDFOREACH()
  FILE(GLOB_RECURSE OBJECTS dtrace_objects_merge/*.o)
  CONVERT_TO_RELATIVE_PATHS("${OBJECTS}" REL_OBJECTS)
  EXECUTE_PROCESS(
     COMMAND ${DTRACE} ${DTRACE_FLAGS} -o ${OUTFILE}  -G -s ${DFILE}  ${REL_OBJECTS}
  )
  RETURN()
ENDIF()

FOREACH(dir ${DIRS})
  FILE(GLOB_RECURSE OBJECTS  ${dir}/*.o)
  CONVERT_TO_RELATIVE_PATHS("${OBJECTS}" REL)
  LIST(APPEND REL_OBJECTS ${REL})
ENDFOREACH()

FILE(WRITE  dtrace_timestamp "")
EXECUTE_PROCESS(
 COMMAND ${DTRACE} ${DTRACE_FLAGS} -o ${OUTFILE}  -G -s ${DFILE}  ${REL_OBJECTS}
)

# Save objects that contain dtrace probes in a file.
# This file is used when script is called with -DTYPE=MERGE
# to dtrace from static libs.
# To find objects with probes, look at the timestamp, it was updated
# by dtrace -G run
IF(TYPE MATCHES "STATIC")
  FILE(WRITE dtrace_objects "")
  FOREACH(obj  ${REL_OBJECTS})
    IF(${obj} IS_NEWER_THAN dtrace_timestamp)
      GET_FILENAME_COMPONENT(obj_absolute_path ${obj} ABSOLUTE)
      FILE(APPEND dtrace_objects "${obj_absolute_path}\n" )
    ENDIF()
  ENDFOREACH()
ENDIF()
