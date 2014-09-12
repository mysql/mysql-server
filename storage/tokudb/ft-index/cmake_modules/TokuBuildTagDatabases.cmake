## set up lists of sources and headers for tags
file(GLOB_RECURSE all_srcs
  buildheader/*.cc
  db-benchmark-test/*.cc
  ft/*.cc
  include/*.cc
  locktree/*.cc
  portability/*.cc
  src/*.cc
  toku_include/*.cc
  utils/*.cc
  util/*.cc
  db-benchmark-test/*.cc
  )
list(APPEND all_srcs
  ${CMAKE_CURRENT_BINARY_DIR}/ft/log_code.cc
  ${CMAKE_CURRENT_BINARY_DIR}/ft/log_print.cc
  )
file(GLOB_RECURSE all_hdrs
  buildheader/*.h
  db-benchmark-test/*.h
  ft/*.h
  include/*.h
  locktree/*.h
  portability/*.h
  src/*.h
  toku_include/*.h
  utils/*.h
  util/*.h
  db-benchmark-test/*.h
  )
list(APPEND all_hdrs
  ${CMAKE_CURRENT_BINARY_DIR}/toku_include/toku_config.h
  ${CMAKE_CURRENT_BINARY_DIR}/buildheader/db.h
  ${CMAKE_CURRENT_BINARY_DIR}/ft/log_header.h
  )

option(USE_CTAGS "Build the ctags database." ON)
if (USE_CTAGS AND
    # Macs by default are not case-sensitive, so tags and TAGS clobber each other.  Do etags and not ctags in that case, because Emacs is superior. :P
    (NOT APPLE OR NOT USE_ETAGS))
  find_program(CTAGS "ctags")
  if (NOT CTAGS MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/tags"
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      COMMAND ${CTAGS} -o tags ${all_srcs} ${all_hdrs}
      COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h generate_log_code
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
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h generate_log_code
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_etags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TAGS" etags-stamp)
  endif ()
endif ()

option(USE_CSCOPE "Build the cscope database." ON)
if (USE_CSCOPE)
  find_program(CSCOPE "cscope")
  if (NOT CSCOPE MATCHES NOTFOUND)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cscope.files" "")
    foreach(file ${all_srcs} ${all_hdrs})
      file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/cscope.files" "${file}\n")
    endforeach(file)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.out"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.in.out"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.po.out"
      COMMAND ${CSCOPE} -b -q -R -i"${CMAKE_CURRENT_BINARY_DIR}/cscope.files" -I"${CMAKE_CURRENT_SOURCE_DIR}" -I"${CMAKE_CURRENT_SOURCE_DIR}/include" -I"${CMAKE_CURRENT_SOURCE_DIR}/toku_include" -I"${CMAKE_CURRENT_SOURCE_DIR}/portability" -I"${CMAKE_CURRENT_SOURCE_DIR}/ft" -I"${CMAKE_CURRENT_SOURCE_DIR}/src" -I"${CMAKE_CURRENT_SOURCE_DIR}/locktree" -I"${CMAKE_CURRENT_SOURCE_DIR}/utils" -I"${CMAKE_CURRENT_SOURCE_DIR}/db-benchmark-test" -I"${CMAKE_CURRENT_BINARY_DIR}" -I"${CMAKE_CURRENT_BINARY_DIR}/toku_include" -I"${CMAKE_CURRENT_BINARY_DIR}/buildheader"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h generate_log_code
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
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gtags.files" "")
    foreach(file ${all_srcs} ${all_hdrs})
      file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/gtags.files" "${file}\n")
    endforeach(file)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GPATH"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GSYMS"
      COMMAND ${GTAGS} -f "${CMAKE_CURRENT_BINARY_DIR}/gtags.files"
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h generate_log_code
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_GTAGS ALL DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
      "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
      "${CMAKE_CURRENT_SOURCE_DIR}/GPATH"
      "${CMAKE_CURRENT_SOURCE_DIR}/GSYMS")
  endif ()
endif ()

option(USE_MKID "Build the idutils database." ON)
if (USE_MKID)
  find_program(MKID "mkid")
  if (NOT MKID MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/ID"
      COMMAND ${MKID} ${all_srcs} ${all_hdrs}
      DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_config_h generate_log_code
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_MKID ALL DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/ID")
  endif ()
endif ()
