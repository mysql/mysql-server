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

INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR} )
INCLUDE(CheckIncludeFile)
CHECK_INCLUDE_FILES(term.h HAVE_TERM_H)

SET(CMAKE_REQUIRED_LIBRARIES ${CURSES_CURSES_LIBRARY})
CHECK_CXX_SOURCE_COMPILES("
#include <term.h>
int main()
{
  tgoto(0,0,0);
  return 0;
}" HAVE_DECL_TGOTO)
SET(CMAKE_REQUIRED_LIBRARIES)



IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  #On Solaris, default awk is next to unusable while the xpg4 one is ok.
  IF(EXISTS /usr/xpg4/bin/awk)
    SET(AWK_EXECUTABLE /usr/xpg4/bin/awk)
  ENDIF()
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "OS400")
  #Workaround for cases, where /usr/bin/gawk is not executable
  IF(EXISTS /QOpenSys/usr/bin/awk)
    SET(AWK_EXECUTABLE /QOpenSys/usr/bin/awk)
  ENDIF()
ENDIF()

IF(NOT AWK_EXECUTABLE)
  FIND_PROGRAM(AWK_EXECUTABLE NAMES gawk awk DOC "path to the awk executable")
ENDIF()

MARK_AS_ADVANCED(AWK_EXECUTABLE)
SET(AWK ${AWK_EXECUTABLE})
CONFIGURE_FILE(makelist.sh ${CMAKE_CURRENT_SOURCE_DIR}/makelist @ONLY)


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CURSES_INCLUDE_PATH}
                    }
)

SET(ASRC vi.c emacs.c common.c)
SET(AHDR vi.h emacs.h common.h)


SET(LIBEDIT_SOURCES
 chared.c el.c history.c map.c prompt.c readline.c 
 search.c tokenizer.c vi.c common.c emacs.c 
 hist.c key.c parse.c read.c refresh.c sig.c term.c
 tty.c help.c fcns.c filecomplete.c
 ${AHDR}
 )

include(CheckFunctionExists)
include(CheckIncludeFile)
CHECK_INCLUDE_FILE(vis.h HAVE_VIS_H)

IF(HAVE_VIS_H)
  CHECK_FUNCTION_EXISTS(strvis HAVE_STRVIS)
  IF(NOT HAVE_STRVIS)
   SET(HAVE_VIS_H FALSE CACHE INTERNAL "" FORCE)
  ENDIF()
ENDIF()

CHECK_FUNCTION_EXISTS(strvis HAVE_STRVIS)
IF(NOT HAVE_STRVIS)
  SET(LIBEDIT_SOURCES ${LIBEDIT_SOURCES}  np/vis.c)
ENDIF()


CHECK_FUNCTION_EXISTS(strunvis HAVE_STRUNVIS)
IF(NOT HAVE_STRUNVIS)
  SET(LIBEDIT_SOURCES ${LIBEDIT_SOURCES} np/unvis.c)
ENDIF()

CHECK_FUNCTION_EXISTS(strlcpy HAVE_STRLCPY)
IF(NOT HAVE_STRLCPY)
  SET(LIBEDIT_SOURCES ${LIBEDIT_SOURCES} np/strlcpy.c)
ENDIF()


CHECK_FUNCTION_EXISTS(strlcat HAVE_STRLCAT)
IF(NOT HAVE_STRLCAT)
  SET(LIBEDIT_SOURCES ${LIBEDIT_SOURCES} np/strlcat.c)
ENDIF()


CHECK_FUNCTION_EXISTS(fgetln HAVE_FGETLN)
IF(NOT HAVE_FGETLN)
  SET(LIBEDIT_SOURCES ${LIBEDIT_SOURCES} np/fgetln.c)
ENDIF()


ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/vi.h
	COMMAND sh ./makelist -h vi.c > vi.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS vi.c)

ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/emacs.h
	COMMAND sh ./makelist -h emacs.c > emacs.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS emacs.c)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/common.h
	COMMAND sh ./makelist -h common.c > common.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS common.c)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help.c
	COMMAND sh ./makelist -bc ${ASRC} > help.c
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ${ASRC}
	)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help.h
	COMMAND sh ./makelist -bh ${ASRC} > help.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ${ASRC}
	)

ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/fcns.h
	COMMAND sh ./makelist -fh ${AHDR} > fcns.h
	VERBATIM
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ${AHDR}
	)

ADD_CUSTOM_COMMAND(OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/fcns.c
	COMMAND sh ./makelist -fc ${AHDR} > fcns.c
        VERBATIM
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ${AHDR}
	)


ADD_CUSTOM_TARGET( 
        GenLibeditSource
        DEPENDS 
        ${CMAKE_CURRENT_SOURCE_DIR}/vi.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/emacs.h
        ${CMAKE_CURRENT_SOURCE_DIR}/common.h
        ${CMAKE_CURRENT_SOURCE_DIR}/help.c
        ${CMAKE_CURRENT_SOURCE_DIR}/help.h
        ${CMAKE_CURRENT_SOURCE_DIR}/fcns.c
        ${CMAKE_CURRENT_SOURCE_DIR}/fcns.h
)

ADD_LIBRARY(edit ${LIBEDIT_SOURCES})
ADD_DEPENDENCIES(edit GenLibeditSource)
TARGET_LINK_LIBRARIES(edit ${CURSES_LIBRARY})

