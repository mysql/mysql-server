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

# Check if OS supports DTrace
MACRO(CHECK_DTRACE)
 FIND_PROGRAM(DTRACE dtrace)
 MARK_AS_ADVANCED(DTRACE)

 # On FreeBSD, dtrace does not handle userland tracing yet
 IF(DTRACE AND NOT CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   SET(ENABLE_DTRACE ON CACHE BOOL "Enable dtrace")
 ENDIF()
 SET(HAVE_DTRACE ${ENABLE_DTRACE})
 IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
   IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
     SET(DTRACE_FLAGS -32 CACHE INTERNAL "DTrace architecture flags")
   ELSE()
     SET(DTRACE_FLAGS -64 CACHE INTERNAL "DTrace architecture flags")
   ENDIF()
 ENDIF()
ENDMACRO()

CHECK_DTRACE()

# Produce a header file  with
# DTrace macros
MACRO (DTRACE_HEADER provider header header_no_dtrace)
 IF(ENABLE_DTRACE)
 ADD_CUSTOM_COMMAND(
   OUTPUT  ${header} ${header_no_dtrace}
   COMMAND ${DTRACE} -h -s ${provider} -o ${header}
	 COMMAND perl ${CMAKE_SOURCE_DIR}/scripts/dheadgen.pl -f ${provider} > ${header_no_dtrace}
   DEPENDS ${provider}
 )
 ENDIF()
ENDMACRO()


# Create provider headers
IF(ENABLE_DTRACE)
  CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/include/probes_mysql.d.base 
    ${CMAKE_BINARY_DIR}/include/probes_mysql.d COPYONLY)
  DTRACE_HEADER(
   ${CMAKE_BINARY_DIR}/include/probes_mysql.d 
   ${CMAKE_BINARY_DIR}/include/probes_mysql_dtrace.h
   ${CMAKE_BINARY_DIR}/include/probes_mysql_nodtrace.h
  )
  ADD_CUSTOM_TARGET(gen_dtrace_header
  DEPENDS  
  ${CMAKE_BINARY_DIR}/include/probes_mysql.d
  ${CMAKE_BINARY_DIR}/include/probes_mysql_dtrace.h
  ${CMAKE_BINARY_DIR}/include/probes_mysql_nodtrace.h
  ) 
ENDIF()


MACRO (DTRACE_INSTRUMENT target)
  IF(ENABLE_DTRACE)
    ADD_DEPENDENCIES(${target} gen_dtrace_header)

     # On Solaris, invoke dtrace -G to generate object file and
     # link it together with target.
    IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
      SET(objdir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir)
      SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${target}_dtrace.o)
  
      ADD_CUSTOM_COMMAND(
        TARGET ${target} PRE_LINK 
        COMMAND ${CMAKE_COMMAND}
          -DDTRACE=${DTRACE}	  
          -DOUTFILE=${outfile} 
          -DDFILE=${CMAKE_BINARY_DIR}/include/probes_mysql.d
          -DDTRACE_FLAGS=${DTRACE_FLAGS}
          -P ${CMAKE_SOURCE_DIR}/cmake/dtrace_prelink.cmake
        WORKING_DIRECTORY ${OBJDIR}
      )
      SET_TARGET_PROPERTIES(${target} PROPERTIES LINK_FLAGS "${outfile}")
    ENDIF()
  ENDIF()
ENDMACRO()
