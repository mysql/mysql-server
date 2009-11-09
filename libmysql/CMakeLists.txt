# Copyright (C) 2006 MySQL AB
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/libmysql
  ${CMAKE_SOURCE_DIR}/regex
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/strings
  ${SSL_INCLUDE_DIRS}
  ${SSL_INTERNAL_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR})
ADD_DEFINITIONS(${SSL_DEFINES})


#Remove -fno-implicit-templates 
#(yassl sources cannot  be compiled with  it)
STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ADD_DEFINITIONS(-DDISABLE_DTRACE)



SET(CLIENT_SOURCES
  get_password.c 
  libmysql.c
  errmsg.c
  ../sql-common/client.c 
  ../sql-common/my_time.c 
  ../sql/net_serv.cc
  ../sql-common/pack.c 
  ../sql/password.c
)

ADD_LIBRARY(clientlib STATIC ${CLIENT_SOURCES})
ADD_DEPENDENCIES(clientlib GenError)

# Merge several static libraries into one big mysqlclient.
SET(LIBS dbug strings vio mysys ${ZLIB_LIBRARY} ${SSL_LIBRARIES})
MERGE_STATIC_LIBS(mysqlclient mysqlclient "${LIBS};clientlib")

ADD_DEPENDENCIES(mysqlclient GenError)
SET_TARGET_PROPERTIES(mysqlclient PROPERTIES CLEAN_DIRECT_OUTPUT 1)


# Make shared client library
IF(WIN32)
  SET(SHARED_OUTPUT_NAME libmysql)
ELSE()
  SET(SHARED_OUTPUT_NAME mysqlclient)
ENDIF()

# On Windows, we can make a shared library out of static.
# On Unix, we need to recompile all sources, unless we compiled with -fPIC, in
# which case we can link static libraries to shared.
IF(MSVC)
  STATIC_TO_SHARED(mysqlclient libmysql libmysql.def)
ELSE()
  SET(LIBMYSQL_SOURCES  ${CLIENT_SOURCES})
  
  IF(NOT WITH_PIC)
    # Add all sources that come into common static libs.
    FOREACH(LIB ${LIBS})
      GET_TARGET_PROPERTY(SRC ${LIB} SOURCES)
      IF (NOT SRC)
        # This must be system shared lib (zlib or openssl)
	    # Users of libmysql must link with it too.
	    LIST(APPEND OS_LIBS ${LIB})
      ELSE()
	    LIST(APPEND LIBMYSQL_SOURCES ${SRC})
	  ENDIF()
    ENDFOREACH()
	
    # Some extra flags as in mysys	
    IF(CMAKE_COMPILER_IS_GNUCC AND  NOT HAVE_CXX_NEW)
      SET_SOURCE_FILES_PROPERTIES(${CMAKE_SOURCE_DIR}/mysys/my_new.cc
      PROPERTIES COMPILE_FLAGS "-DUSE_MYSYS_NEW")
    ENDIF()
  ENDIF()

  
  ADD_LIBRARY(libmysql SHARED ${LIBMYSQL_SOURCES})
  ADD_DEPENDENCIES(libmysql GenError)
  SET_TARGET_PROPERTIES(libmysql PROPERTIES OUTPUT_NAME ${SHARED_OUTPUT_NAME}
    SOVERSION "${SHARED_LIB_MAJOR_VERSION}.0") 
  SET_TARGET_PROPERTIES(libmysql PROPERTIES CLEAN_DIRECT_OUTPUT 1)
  
  IF(WITH_PIC)
   TARGET_LINK_LIBRARIES(libmysql ${LIBS})
  ENDIF()
  
  IF(OS_LIBS)
    TARGET_LINK_LIBRARIES(libmysql ${OS_LIBS})
  ENDIF()
ENDIF()


IF(UNIX)
 # Install links to shared and static libraries
 # (append _r to base name)
 INSTALL_SYMLINK(${CMAKE_SHARED_LIBRARY_PREFIX}mysqlclient_r libmysql lib)
 INSTALL_SYMLINK(${CMAKE_STATIC_LIBRARY_PREFIX}mysqlclient_r mysqlclient lib)
ENDIF()

INSTALL(TARGETS mysqlclient libmysql DESTINATION lib)
INSTALL_DEBUG_SYMBOLS( "mysqlclient;libmysql")
