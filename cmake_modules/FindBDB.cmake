# - Try to find BDB
# Once done this will define
#  BDB_FOUND - System has BDB
#  BDB_INCLUDE_DIRS - The BDB include directories
#  BDB_LIBRARIES - The libraries needed to use BDB
#  BDB_DEFINITIONS - Compiler switches required for using BDB

find_path(BDB_INCLUDE_DIR db.h)

find_library(BDB_LIBRARY NAMES db libdb)

include(CheckSymbolExists)
## check if the found bdb has DB_TXN_SNAPSHOT
set(CMAKE_REQUIRED_INCLUDES ${BDB_INCLUDE_DIR})
check_symbol_exists(DB_TXN_SNAPSHOT "db.h" HAVE_DB_TXN_SNAPSHOT)
if(HAVE_DB_TXN_SNAPSHOT)
  set(BDB_INCLUDE_DIRS ${BDB_INCLUDE_DIR})
  set(BDB_LIBRARIES ${BDB_LIBRARY})

  include(FindPackageHandleStandardArgs)
  # handle the QUIETLY and REQUIRED arguments and set BDB_FOUND to TRUE
  # if all listed variables are TRUE
  find_package_handle_standard_args(BDB DEFAULT_MSG
                                    BDB_LIBRARY BDB_INCLUDE_DIR)

  mark_as_advanced(BDB_INCLUDE_DIR BDB_LIBRARY)
endif()
