# Copyright (c) 2009, 2016, Oracle and/or its affiliates. All rights reserved.
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

 IF(ENABLE_DTRACE AND NOT DTRACE)
   MESSAGE(FATAL_ERROR "Could not find dtrace")
 ENDIF()

 # On FreeBSD, dtrace does not handle userland tracing yet
 IF(DTRACE AND NOT CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   SET(ENABLE_DTRACE ON CACHE BOOL "Enable dtrace")
 ENDIF()
 SET(HAVE_DTRACE ${ENABLE_DTRACE})
 EXECUTE_PROCESS(
   COMMAND ${DTRACE} -V
   OUTPUT_VARIABLE out)
 IF(out MATCHES "Sun D" OR out MATCHES "Oracle D")
   IF(NOT CMAKE_SYSTEM_NAME MATCHES "FreeBSD" AND
      NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
     SET(HAVE_REAL_DTRACE_INSTRUMENTING ON CACHE BOOL "Real DTrace detected")
   ENDIF()
 ENDIF()
 IF(HAVE_REAL_DTRACE_INSTRUMENTING)
   IF(SIZEOF_VOIDP EQUAL 4)
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

FUNCTION(DTRACE_INSTRUMENT target)
  IF(ENABLE_DTRACE)
    ADD_DEPENDENCIES(${target} gen_dtrace_header)

    # Invoke dtrace to generate object file and link it together with target.
    IF(HAVE_REAL_DTRACE_INSTRUMENTING)
      SET(objdir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir)
      SET(outfile ${objdir}/${target}_dtrace.o)
      GET_TARGET_PROPERTY(target_type ${target} TYPE)
      ADD_CUSTOM_COMMAND(
        TARGET ${target} PRE_LINK 
        COMMAND ${CMAKE_COMMAND}
          -DDTRACE=${DTRACE}	  
          -DOUTFILE=${outfile} 
          -DDFILE=${CMAKE_BINARY_DIR}/include/probes_mysql.d
          -DDTRACE_FLAGS=${DTRACE_FLAGS}
          -DDIRS=.
          -DTYPE=${target_type}
          -P ${CMAKE_SOURCE_DIR}/cmake/dtrace_prelink.cmake
        WORKING_DIRECTORY ${objdir}
      )
    ELSEIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
      # dtrace on Linux runs gcc and uses flags from environment
      SET(CFLAGS_SAVED $ENV{CFLAGS})
      # We want to strip off all warning flags, including -Werror=xxx-xx,
      # but keep flags like
      #  -Wp,-D_FORTIFY_SOURCE=2
      STRING(REGEX REPLACE "-W[A-Za-z0-9][-A-Za-z0-9=]+" ""
        C_FLAGS "${CMAKE_C_FLAGS}")

      SET(ENV{CFLAGS} ${C_FLAGS})
      SET(outfile "${CMAKE_BINARY_DIR}/probes_mysql.o")
      # Systemtap object
      EXECUTE_PROCESS(
        COMMAND ${DTRACE} -G -s ${CMAKE_SOURCE_DIR}/include/probes_mysql.d.base
        -o ${outfile}
        )
      SET(ENV{CFLAGS} ${CFLAGS_SAVED})
    ENDIF()

    # Do not try to extend the library if we have not built the .o file
    IF(outfile)
      # Add full  object path to linker flags
      GET_TARGET_PROPERTY(target_type ${target} TYPE)
      IF(NOT target_type MATCHES "STATIC")
        SET_TARGET_PROPERTIES(${target} PROPERTIES LINK_FLAGS "${outfile}")
      ELSE()
        # For static library flags, add the object to the library.
        GET_TARGET_PROPERTY(target_location ${target} LOCATION)
        ADD_CUSTOM_COMMAND(
          TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_AR} r  ${target_location} ${outfile}
	  COMMAND ${CMAKE_RANLIB} ${target_location}
          )
      ENDIF()
    ENDIF()
  ENDIF()
ENDFUNCTION()
