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

# Generates an ELF object file with dtrace entry points.
# This object that must to be linked with together with 
# the target. This script needs to run on Solaris only

# Do not follow symlinks in GLOB_RECURSE
CMAKE_POLICY(SET CMP0009 NEW)
FILE(GLOB_RECURSE OBJECTS  *.o)

#  Use relative paths to generate shorter command line
GET_FILENAME_COMPONENT(CURRENT_ABS_DIR . ABSOLUTE)
FOREACH(OBJ ${OBJECTS})
  FILE(RELATIVE_PATH REL  ${CURRENT_ABS_DIR} ${OBJ})
  LIST(APPEND REL_OBJECTS  ${REL})
ENDFOREACH()

EXECUTE_PROCESS(
 COMMAND ${DTRACE} ${DTRACE_FLAGS} -o ${OUTFILE}  -G -s ${DFILE}  ${REL_OBJECTS}
)

