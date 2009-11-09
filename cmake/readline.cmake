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

MACRO(SET_WITH_BUNDLED_READLINE option)
  IF(option)
   SET(not_option OFF)
  ELSE()
   SET(not_option ON)
  ENDIF()
  SET(WITH_READLINE ${option} CACHE BOOL "Use bundled readline")
  SET(WITH_LIBEDIT ${not_option} CACHE BOOL "Use bundled libedit")
ENDMACRO()

MACRO (MYSQL_CHECK_MULTIBYTE)
  CHECK_INCLUDE_FILE(wctype.h HAVE_WCTYPE_H)
  CHECK_INCLUDE_FILE(wchar.h HAVE_WCHAR_H)
  IF(HAVE_WCHAR_H)
    SET(CMAKE_EXTRA_INCLUDE_FILES wchar.h)
    CHECK_TYPE_SIZE(mbstate_t SIZEOF_MBSTATE_T)
    SET(CMAKE_EXTRA_INCLUDE_FILES)
    IF(SIZEOF_MBSTATE_T)
      SET(HAVE_MBSTATE_T 1)
    ENDIF()
  ENDIF()

  CHECK_C_SOURCE_COMPILES("
  #include <langinfo.h>
  int main(int ac, char **av)
  {
    char *cs = nl_langinfo(CODESET);
    return 0;
  }"
  HAVE_LANGINFO_CODESET)
  
  CHECK_FUNCTION_EXISTS(mbrlen HAVE_MBRLEN)
  CHECK_FUNCTION_EXISTS(mbscmp HAVE_MBSCMP)
  CHECK_FUNCTION_EXISTS(mbsrtowcs HAVE_MBSRTOWCS)
  CHECK_FUNCTION_EXISTS(wcrtomb HAVE_WCRTOMB)
  CHECK_FUNCTION_EXISTS(mbrtowc HAVE_MBRTOWC)
  CHECK_FUNCTION_EXISTS(wcscoll HAVE_WCSCOLL)
  CHECK_FUNCTION_EXISTS(wcsdup HAVE_WCSDUP)
  CHECK_FUNCTION_EXISTS(wcwidth HAVE_WCWIDTH)
  CHECK_FUNCTION_EXISTS(wctype HAVE_WCTYPE)
  CHECK_FUNCTION_EXISTS(iswlower HAVE_ISWLOWER)
  CHECK_FUNCTION_EXISTS(iswupper HAVE_ISWUPPER)
  CHECK_FUNCTION_EXISTS(towlower HAVE_TOWLOWER)
  CHECK_FUNCTION_EXISTS(towupper HAVE_TOWUPPER)
  CHECK_FUNCTION_EXISTS(iswctype HAVE_ISWCTYPE)

  SET(CMAKE_EXTRA_INCLUDE_FILES wchar.h)
  CHECK_TYPE_SIZE(wchar_t SIZEOF_WCHAR_T)
  IF(SIZEOF_WCHAR_T)
    SET(HAVE_WCHAR_T 1)
  ENDIF()

  SET(CMAKE_EXTRA_INCLUDE_FILES wctype.h)
  CHECK_TYPE_SIZE(wctype_t SIZEOF_WCTYPE_T)
  IF(SIZEOF_WCTYPE_T)
    SET(HAVE_WCTYPE_T 1)
  ENDIF()
  CHECK_TYPE_SIZE(wint_t SIZEOF_WINT_T)
  IF(SIZEOF_WINT_T)
    SET(HAVE_WINT_T 1)
  ENDIF()
  SET(CMAKE_EXTRA_INCLUDE_FILES)

ENDMACRO()

MACRO (FIND_CURSES)
 INCLUDE (FindCurses)
 MARK_AS_ADVANCED(CURSES_CURSES_H_PATH CURSES_FORM_LIBRARY CURSES_HAVE_CURSES_H)
 IF(NOT CURSES_FOUND)
   MESSAGE(FATAL_ERROR "curses library not found")
 ENDIF()

 IF(CURSES_HAVE_CURSES_H)
   SET(HAVE_CURSES_H 1 CACHE INTERNAL "")
 ELSEIF(CURSES_HAVE_NCURSES_H)
   SET(HAVE_NCURSES_H 1 CACHE INTERNAL "")
 ENDIF()
 IF(CMAKE_SYSTEM_NAME MATCHES "HP")
   # CMake uses full path to library /lib/libcurses.sl 
   # On Itanium, it results into architecture mismatch+
   # the library is for  PA-RISC
   SET(CURSES_LIBRARY "curses" CACHE INTERNAL "" FORCE)
   SET(CURSES_CURSES_LIBRARY "curses" CACHE INTERNAL "" FORCE)
 ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_READLINE)
  SET_WITH_BUNDLED_READLINE(ON)
  SET(USE_NEW_READLINE_INTERFACE 1)
  SET(READLINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/cmd-line-utils)
  SET(READLINE_LIBRARY readline)
  FIND_CURSES()
  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/cmd-line-utils/readline)
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LIBEDIT)
  SET_WITH_BUNDLED_READLINE(OFF)
  SET(USE_LIBEDIT_INTERFACE 1 CACHE INTERNAL "")
  SET(HAVE_HIST_ENTRY 1 CACHE INTERNAL "")
  SET(READLINE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/cmd-line-utils/libedit")
  SET(READLINE_LIBRARY edit)
  FIND_CURSES()
  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/cmd-line-utils/libedit)
ENDMACRO()

MACRO (MYSQL_FIND_SYSTEM_READLINE name)
  FIND_PATH(SYSTEM_READLINE_INCLUDE_DIR readline/readline.h )
  FIND_LIBRARY(SYSTEM_READLINE_LIBRARY NAMES ${name})
  MARK_AS_ADVANCED(SYSTEM_READLINE_INCLUDE_DIR  SYSTEM_READLINE_LIBRARY)

  INCLUDE(CheckCXXSourceCompiles)
  SET(CMAKE_REQUIRES_LIBRARIES ${SYSTEM_READLINE_LIBRARY})

  IF(SYSTEM_READLINE_LIBRARY AND SYSTEM_READLINE_INCLUDE_DIR)
    SET(SYSTEM_READLINE_FOUND 1)
    SET(CMAKE_REQUIRED_LIBRARIES ${SYSTEM_READLINE_LIBRARY})
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
       HIST_ENTRY entry;
       return 0;
    }"
    HAVE_HIST_ENTRY)

    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
      char res= *(*rl_completion_entry_function)(0,0);
      completion_matches(0,0);
    }"
    USE_LIBEDIT_INTERFACE)


    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
      rl_completion_func_t *func1= (rl_completion_func_t*)0;
      rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
    }"
    USE_NEW_READLINE_INTERFACE)
    
    IF(USE_LIBEDIT_INTERFACE  OR USE_NEW_READLINE_INTERFACE)
      SET(READLINE_LIBRARY ${SYSTEM_READLINE_LIBRARY})
      SET(READLINE_INCLUDE_DIR ${SYSTEM_READLINE_INCLUDE_DIR})
      SET(READLINE_FOUND 1)
    ENDIF()
  ENDIF()
ENDMACRO()

MACRO (MYSQL_CHECK_READLINE)
  IF (NOT WIN32)
    MYSQL_CHECK_MULTIBYTE()
    OPTION(WITH_READLINE "Use bundled readline" OFF)
    IF(NOT CYGWIN)	
      # Bundled libedit does not compile on cygwin
      OPTION(WITH_LIBEDIT  "Use bundled libedit" ON)
    ELSE()
      OPTION(WITH_LIBEDIT  "Use bundled libedit" OFF)
    ENDIF()

    IF(WITH_READLINE)
     MYSQL_USE_BUNDLED_READLINE()
    ELSEIF(WITH_LIBEDIT)
     MYSQL_USE_BUNDLED_LIBEDIT()
    ELSE()
      MYSQL_FIND_SYSTEM_READLINE(readline)
      IF(NOT READLINE_FOUND)
        MYSQL_FIND_SYSTEM_READLINE(edit)
        IF(NOT READLINE_FOUND)
          MESSAGE(FATAL_ERROR "Cannot find system readline or libedit libraries.Use WITH_READLINE or WITH_LIBEDIT")
        ENDIF()
      ENDIF()
    ENDIF()
  ENDIF(NOT WIN32)
ENDMACRO()

