## set up lists of sources and headers for tags
file(GLOB_RECURSE all_srcs
  include/*.c
  toku_include/*.c
  buildheader/*.c
  portability/*.c
  newbrt/*.c
  src/*.c
  utils/*.c
  db-benchmark-test/*.c
  ${CMAKE_CURRENT_BINARY_DIR}/newbrt/log_code.c
  ${CMAKE_CURRENT_BINARY_DIR}/newbrt/log_print.c
  )
file(GLOB_RECURSE all_hdrs
  include/*.h
  toku_include/*.h
  buildheader/*.h
  portability/*.h
  newbrt/*.h
  src/*.h
  utils/*.h
  db-benchmark-test/*.h
  ${CMAKE_CURRENT_BINARY_DIR}/toku_include/config.h
  ${CMAKE_CURRENT_BINARY_DIR}/buildheader/db.h
  ${CMAKE_CURRENT_BINARY_DIR}/newbrt/log_header.h
  )

option(USE_CTAGS "Build the ctags database." ON)
if (USE_CTAGS)
  find_program(CTAGS "ctags")
  if (NOT CTAGS MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/tags"
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      COMMAND ${CTAGS} -o tags ${all_srcs} ${all_hdrs}
      COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_ctags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tags" ctags-stamp)
  endif ()
endif ()

option(USE_ETAGS "Build the etags database." ON)
if (USE_ETAGS)
  find_program(ETAGS "etags")
  if (NOT ETAGS MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/TAGS"
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
      COMMAND ${ETAGS} -o TAGS ${all_srcs} ${all_hdrs}
      COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_etags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TAGS" etags-stamp)
  endif ()
endif ()

option(USE_CSCOPE "Build the cscope database." ON)
if (USE_CSCOPE)
  find_program(CSCOPE "cscope")
  if (NOT CSCOPE MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.out"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.in.out"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.po.out"
      COMMAND ${CSCOPE} -b -q -R
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_cscope.out ALL DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/cscope.out"
      "${CMAKE_CURRENT_SOURCE_DIR}/cscope.in.out"
      "${CMAKE_CURRENT_SOURCE_DIR}/cscope.po.out")
  endif ()
endif ()

option(USE_GTAGS "Build the gtags database." ON)
if (USE_GTAGS)
  find_program(GTAGS "gtags")
  if (NOT GTAGS MATCHES NOTFOUND)
    ## todo: use global -u instead of gtags each time
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GPATH"
      COMMAND ${GTAGS} --gtagsconf "${CMAKE_CURRENT_SOURCE_DIR}/.globalrc"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_GTAGS ALL DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
      "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
      "${CMAKE_CURRENT_SOURCE_DIR}/GPATH")
  endif ()
endif ()