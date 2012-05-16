if(BUILD_TESTING)
  set(MEMORYCHECK_COMMAND_OPTIONS "--gen-suppressions=no --num-callers=20 --leak-check=full --show-reachable=yes --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,test,*/tokudb_dump* --trace-children-skip-by-arg=--only_create,--test,--no-shutdown,novalgrind" CACHE INTERNAL "options for valgrind")
  set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" CACHE INTERNAL "suppressions file for valgrind")
endif()
