#
# Generate LIBS and CFLAGS that third-party clients should use
#

# Use cmake variables to inspect dependencies for 
# mysqlclient library (add -l stuff)
SET(CLIENT_LIBS "")
SET(LIBS "")

# Avoid compatibility warning about lists with empty elements
IF(POLICY CMP0011)
  CMAKE_POLICY(SET CMP0011 NEW)
ENDIF()
IF(POLICY CMP0007)
  CMAKE_POLICY(SET CMP0007 OLD)
ENDIF()

# Extract dependencies using CMake's internal ${target}_LIB_DEPENDS variable
# returned string in ${var} is can be passed to linker's command line
MACRO(EXTRACT_LINK_LIBRARIES target var)
  IF(${target}_LIB_DEPENDS)
    LIST(REMOVE_ITEM ${target}_LIB_DEPENDS "")
    LIST(REMOVE_DUPLICATES ${target}_LIB_DEPENDS)
    FOREACH(lib ${${target}_LIB_DEPENDS})
      # Filter out "general", it is not a library, just CMake hint
      # Also, remove duplicates
      IF(NOT lib STREQUAL "general" AND NOT ${var}  MATCHES "-l${lib} ")
        IF (lib MATCHES "^\\-l")
          SET(${var} "${${var}} ${lib} ") 
        ELSEIF(lib MATCHES "^/")
          IF (lib MATCHES "\\.(a|so([0-9.]*)|lib|dll|dylib)$")
            # Full path, convert to just filename, strip "lib" prefix and extension
            GET_FILENAME_COMPONENT(lib "${lib}" NAME_WE)
            STRING(REGEX REPLACE "^lib" "" lib "${lib}")
            SET(${var} "${${var}}-l${lib} " ) 
          ENDIF()
        ELSE()
          SET(${var} "${${var}}-l${lib} " ) 
        ENDIF()
      ENDIF()
    ENDFOREACH()
  ENDIF()
  IF(MSVC)
    STRING(REPLACE "-l" "" ${var} "${${var}}")
  ENDIF()
ENDMACRO()

EXTRACT_LINK_LIBRARIES(mysqlclient LIBS)
EXTRACT_LINK_LIBRARIES(mysqlserver EMB_LIBS)

SET(LIBS     "-lmysqlclient ${ZLIB_DEPS} ${LIBS} ${openssl_libs}")
SET(EMB_LIBS "-lmysqld ${ZLIB_DEPS} ${EMB_LIBS} ${openssl_libs}")

MACRO(REPLACE_FOR_CLIENTS VAR)
  SET(v " ${${VAR}} ")
  FOREACH(del ${ARGN})
    STRING(REGEX REPLACE " -(${del}) " " " v ${v})
  ENDFOREACH(del)
  STRING(REGEX REPLACE " +" " " v ${v})
  STRING(STRIP "${v}" ${VAR}_FOR_CLIENTS)
ENDMACRO()

# Remove some options that a client doesn't have to care about
# FIXME until we have a --cxxflags, we need to remove -Xa
#       and -xstrconst to make --cflags usable for Sun Forte C++
# FIXME until we have a --cxxflags, we need to remove -AC99
#       to make --cflags usable for HP C++ (aCC)
REPLACE_FOR_CLIENTS(CFLAGS "[DU]DBUG_OFF" "[DU]SAFE_MUTEX" "[DU]NDEBUG"
  "[DU]UNIV_MUST_NOT_INLINE" "[DU]FORCE_INIT_OF_VARS" "[DU]EXTRA_DEBUG" "[DU]HAVE_valgrind"
  "O" "O[0-9]" "xO[0-9]" "W[-A-Za-z]*" "mtune=[-A-Za-z0-9]*" "g" "fPIC"
  "mcpu=[-A-Za-z0-9]*" "unroll2" "ip" "mp" "march=[-A-Za-z0-9]*" "Xa"
  "xstrconst" "xc99=none" "AC99" "restrict")

# Same for --libs
REPLACE_FOR_CLIENTS(LIBS lmtmalloc static-libcxa i-static static-intel)
REPLACE_FOR_CLIENTS(EMB_LIBS lmtmalloc static-libcxa i-static static-intel)

