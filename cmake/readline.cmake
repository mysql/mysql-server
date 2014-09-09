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

INCLUDE(CheckCXXSourceCompiles)

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
 CHECK_LIBRARY_EXISTS(${CURSES_LIBRARY} setupterm "" HAVE_SETUPTERM)
 CHECK_LIBRARY_EXISTS(${CURSES_LIBRARY} vidattr "" HAVE_VIDATTR)
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_READLINE)
  SET(USE_NEW_READLINE_INTERFACE 1)
  SET(HAVE_HIST_ENTRY 0 CACHE INTERNAL "" FORCE)
  SET(MY_READLINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/cmd-line-utils/readline)
  SET(MY_READLINE_LIBRARY readline)
  ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/cmd-line-utils/readline)
ENDMACRO()

MACRO (MYSQL_FIND_SYSTEM_READLINE)
  
  FIND_PATH(READLINE_INCLUDE_DIR readline.h PATH_SUFFIXES readline)
  FIND_LIBRARY(READLINE_LIBRARY NAMES readline)
  MARK_AS_ADVANCED(READLINE_INCLUDE_DIR READLINE_LIBRARY)

  IF(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
    SET(CMAKE_REQUIRED_LIBRARIES ${READLINE_LIBRARY} ${CURSES_LIBRARY})
    SET(CMAKE_REQUIRED_INCLUDES ${READLINE_INCLUDE_DIR})
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline.h>
    int main(int argc, char **argv)
    {
      rl_completion_func_t *func1= (rl_completion_func_t*)0;
      rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
    }"
    NEW_READLINE_INTERFACE)

    CHECK_C_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline.h>
    #if RL_VERSION_MAJOR > 5
    #error
    #endif
    int main(int argc, char **argv)
    {
      return 0;
    }"
    READLINE_V5)

    IF(NEW_READLINE_INTERFACE)
      IF (READLINE_V5)
        SET(USE_NEW_READLINE_INTERFACE 1)
      ELSE()
        IF(NOT_FOR_DISTRIBUTION)
          SET(NON_DISTRIBUTABLE_WARNING 1)
          SET(USE_NEW_READLINE_INTERFACE 1)
        ELSE()
          SET(USE_NEW_READLINE_INTERFACE 0)
        ENDIF(NOT_FOR_DISTRIBUTION)
      ENDIF(READLINE_V5)
    ENDIF(NEW_READLINE_INTERFACE)
  ENDIF()
ENDMACRO()

MACRO (MYSQL_FIND_SYSTEM_LIBEDIT)
  FIND_PATH(LIBEDIT_INCLUDE_DIR readline.h PATH_SUFFIXES editline edit/readline)
  FIND_LIBRARY(LIBEDIT_LIBRARY edit)
  MARK_AS_ADVANCED(LIBEDIT_INCLUDE_DIR LIBEDIT_LIBRARY)

  IF(LIBEDIT_LIBRARY AND LIBEDIT_INCLUDE_DIR)
    SET(CMAKE_REQUIRED_LIBRARIES ${LIBEDIT_LIBRARY})
    SET(CMAKE_REQUIRED_INCLUDES ${LIBEDIT_INCLUDE_DIR})
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline.h>
    int main(int argc, char **argv)
    {
      int res= (*rl_completion_entry_function)(0,0);
      completion_matches(0,0);
    }"
    LIBEDIT_INTERFACE)
    SET(USE_LIBEDIT_INTERFACE ${LIBEDIT_INTERFACE})
  ENDIF()
ENDMACRO()


MACRO (MYSQL_CHECK_READLINE)
  IF (NOT WIN32)
    MYSQL_CHECK_MULTIBYTE()
    SET(WITH_READLINE OFF CACHE BOOL "Use bundled readline")
    FIND_CURSES()

    IF(WITH_READLINE)
      MYSQL_USE_BUNDLED_READLINE()
    ELSE()
      # OSX includes incompatible readline lib
      IF (NOT APPLE)
        MYSQL_FIND_SYSTEM_READLINE()
      ENDIF()
      IF(USE_NEW_READLINE_INTERFACE)
        SET(MY_READLINE_INCLUDE_DIR ${READLINE_INCLUDE_DIR})
        SET(MY_READLINE_LIBRARY ${READLINE_LIBRARY} ${CURSES_LIBRARY})
      ELSE()
        MYSQL_FIND_SYSTEM_LIBEDIT()
        IF(USE_LIBEDIT_INTERFACE)
          SET(MY_READLINE_INCLUDE_DIR ${LIBEDIT_INCLUDE_DIR})
          SET(MY_READLINE_LIBRARY ${LIBEDIT_LIBRARY} ${CURSES_LIBRARY})
        ELSE()
          MYSQL_USE_BUNDLED_READLINE()
        ENDIF()
      ENDIF()
    ENDIF()

    SET(CMAKE_REQUIRED_LIBRARIES ${MY_READLINE_LIBRARY})
    SET(CMAKE_REQUIRED_INCLUDES ${MY_READLINE_INCLUDE_DIR})
    CHECK_CXX_SOURCE_COMPILES("
    #include <stdio.h>
    #include <readline.h>
    int main(int argc, char **argv)
    {
       HIST_ENTRY entry;
       return 0;
    }"
    HAVE_HIST_ENTRY)
    SET(CMAKE_REQUIRED_LIBRARIES)
    SET(CMAKE_REQUIRED_INCLUDES)
  ENDIF(NOT WIN32)
ENDMACRO()

