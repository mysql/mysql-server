include(CheckSymbolExists)
include(CheckIncludeFiles)

# Check if each symbol in the symbol list exists,
# and define PREFIX__HAVE_SYMNAME to 1 if yes.
#
# SYMLIST: list of symbols to check
# HEADERS: header files to be included in check code
# PREFIX: the prefix of definition
macro(CHECK_SYMBOLS_EXIST SYMLIST HEADERS PREFIX)
  foreach(SYMNAME ${SYMLIST})
    string(TOUPPER "${SYMNAME}" SYMNAME_UPPER)
    if ("${PREFIX}" STREQUAL "")
      set(HAVE_SYM_DEF "HAVE_${SYMNAME_UPPER}")
    else()
      set(HAVE_SYM_DEF "${PREFIX}__HAVE_${SYMNAME_UPPER}")
    endif()
    CHECK_SYMBOL_EXISTS(${SYMNAME} "${HEADERS}" ${HAVE_SYM_DEF})
  endforeach()
endmacro()

# Check if file exists, define PREFIX__HAVE_FILE to 1 if yes,
# and collect file to EVENT_INCLUDES
macro(CHECK_INCLUDE_FILE_CONCAT FILE PREFIX)
  string(REGEX REPLACE "[./]" "_" FILE_UL ${FILE})
  string(TOUPPER "${FILE_UL}" FILE_UL_UPPER)
  if ("${PREFIX}" STREQUAL "")
    set(HAVE_FILE_DEF "HAVE_${FILE_UL_UPPER}")
  else()
    set(HAVE_FILE_DEF "${PREFIX}__HAVE_${FILE_UL_UPPER}")
  endif()
  CHECK_INCLUDE_FILES("${EVENT_INCLUDES};${FILE}" ${HAVE_FILE_DEF})
  if(${HAVE_FILE_DEF})
    set(EVENT_INCLUDES ${EVENT_INCLUDES} ${FILE})
  endif()
endmacro()
