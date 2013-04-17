if(BUILD_TESTING)
  if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
    # Valgrind on OSX 10.8 generally works but outputs some warning junk
    # that is hard to parse out, so we'll just let it run alone
    set(MEMORYCHECK_COMMAND "${TokuDB_SOURCE_DIR}/scripts/tokuvalgrind")
  endif ()
  set(MEMORYCHECK_COMMAND_OPTIONS "--gen-suppressions=no --soname-synonyms=somalloc=*tokuportability* --quiet --num-callers=20 --leak-check=full --show-reachable=yes --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,grep,*/grep,date,*/date,test,*/tokudb_dump,*/tdb-recover --trace-children-skip-by-arg=--only_create,--test,--no-shutdown,novalgrind" CACHE INTERNAL "options for valgrind")
  set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" CACHE INTERNAL "suppressions file for valgrind")
  set(UPDATE_COMMAND "svn")
endif()
