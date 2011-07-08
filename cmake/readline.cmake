# Copyright (c) 2009, 2010 Sun Microsystems, Inc.
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
 FIND_PACKAGE(Curses) 
 MARK_AS_ADVANCED(CURSES_CURSES_H_PATH CURSES_FORM_LIBRARY CURSES_HAVE_CURSES_H)
 IF(NOT CURSES_FOUND)
   SET(ERRORMSG "Curses library not found. Please install appropriate package,
    remove CMakeCache.txt and rerun cmake.")
   IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(ERRORMSG ${ERRORMSG} 
    "On Debian/Ubuntu, package name is libncurses5-dev, on Redhat and derivates " 
    "it is ncurses-devel.")
   ENDIF()
   MESSAGE(FATAL_ERROR ${ERRORMSG})
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

 IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
   # -Wl,--as-needed breaks linking with -lcurses, e.g on Fedora 
   # Lower-level libcurses calls are exposed by libtinfo
   CHECK_LIBRARY_EXISTS(${CURSES_LIBRARY} tputs "" HAVE_TPUTS_IN_CURSES)
   IF(NOT HAVE_TPUTS_IN_CURSES)
     CHECK_LIBRARY_EXISTS(tinfo tputs "" HAVE_TPUTS_IN_TINFO)
     IF(HAVE_TPUTS_IN_TINFO)
       SET(CURSES_LIBRARY tinfo)
     ENDIF()
   ENDIF() 
 ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_READLINE)
  SET(USE_NEW_READLINE_INTERFACE 1)
  SET(HAVE_HIST_ENTRY)
  SET(USE_LIBEDIT_INTERFACE)
  SET(READLINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/cmd-line-utils)
  SET(READLINE_LIBRARY readline)
  FIND_CURSES()
  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/cmd-line-utils/readline)
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LIBEDIT)
  SET(USE_LIBEDIT_INTERFACE 1)
  SET(HAVE_HIST_ENTRY 1)
  SET(READLINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/cmd-line-utils/libedit)
  SET(READLINE_LIBRARY edit)
  FIND_CURSES()
  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/cmd-line-utils/libedit)
ENDMACRO()


MACRO (MYSQL_FIND_SYSTEM_READLINE name)
  
  FIND_PATH(${name}_INCLUDE_DIR readline/readline.h )
  FIND_LIBRARY(${name}_LIBRARY NAMES ${name})
  MARK_AS_ADVANCED(${name}_INCLUDE_DIR  ${name}_LIBRARY)

  INCLUDE(CheckCXXSourceCompiles)
  SET(CMAKE_REQUIRES_LIBRARIES ${${name}_LIBRARY})

  IF(${name}_LIBRARY AND ${name}_INCLUDE_DIR)
    SET(SYSTEM_READLINE_FOUND 1)
    SET(CMAKE_REQUIRED_LIBRARIES ${${name}_LIBRARY})
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
       HIST_ENTRY entry;
       return 0;
    }"
    ${name}_HAVE_HIST_ENTRY)
    
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
      char res= *(*rl_completion_entry_function)(0,0);
      completion_matches(0,0);
    }"
    ${name}_USE_LIBEDIT_INTERFACE)


    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline/readline.h>
    int main(int argc, char **argv)
    {
      rl_completion_func_t *func1= (rl_completion_func_t*)0;
      rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
    }"
    ${name}_USE_NEW_READLINE_INTERFACE)
    
    IF(${name}_USE_LIBEDIT_INTERFACE  OR ${name}_USE_NEW_READLINE_INTERFACE)
      SET(READLINE_LIBRARY ${${name}_LIBRARY})
      SET(READLINE_INCLUDE_DIR ${${name}_INCLUDE_DIR})
      SET(HAVE_HIST_ENTRY ${${name}_HAVE_HIST_ENTRY})
      SET(USE_LIBEDIT_INTERFACE ${${name}_USE_LIBEDIT_INTERFACE})
      SET(USE_NEW_READLINE_INTERFACE ${${name}_USE_NEW_READLINE_INTERFACE})
      SET(READLINE_FOUND 1)
    ENDIF()
  ENDIF()
ENDMACRO()


MACRO (MYSQL_CHECK_READLINE)
  IF (NOT WIN32)
    MYSQL_CHECK_MULTIBYTE()
    IF(NOT CYGWIN)	
      SET(WITH_LIBEDIT  ON CACHE BOOL  "Use bundled libedit")
      SET(WITH_READLINE OFF CACHE BOOL "Use bundled readline")
    ELSE()
      # Bundled libedit does not compile on cygwin, only readline
      SET(WITH_READLINE OFF CACHE BOOL "Use bundled readline")
    ENDIF()

    # Handle mutual exclusion of WITH_READLINE/WITH_LIBEDIT variables
    # We save current setting to recognize when user switched between
    # WITH_READLINE and WITH_LIBEDIT 
    IF(WITH_READLINE)
      IF(NOT SAVE_READLINE_SETTING OR SAVE_READLINE_SETTING MATCHES 
         "WITH_LIBEDIT")
        SET(WITH_LIBEDIT OFF CACHE BOOL "Use bundled libedit" FORCE)
      ENDIF()
    ELSEIF(WITH_LIBEDIT) 
      IF(NOT SAVE_READLINE_SETTING OR SAVE_READLINE_SETTING MATCHES 
         "WITH_READLINE")
        SET(WITH_READLINE OFF CACHE BOOL "Use bundled readline" FORCE)
      ENDIF()
    ENDIF()

    IF(WITH_READLINE)
     MYSQL_USE_BUNDLED_READLINE()
     SET(SAVE_READLINE_SETTING WITH_READLINE CACHE INTERNAL "" FORCE)
    ELSEIF(WITH_LIBEDIT)
     MYSQL_USE_BUNDLED_LIBEDIT()
     SET(SAVE_READLINE_SETTING WITH_LIBEDIT CACHE INTERNAL "" FORCE)
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

