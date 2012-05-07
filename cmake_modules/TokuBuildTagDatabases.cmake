find_program(CTAGS "ctags")
if(NOT CTAGS MATCHES "CTAGS-NOTFOUND")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/tags"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
    COMMAND ${CTAGS} -o tags ${all_srcs} ${all_hdrs}
    COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
    DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_logging_code generate_config_h
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target(build_ctags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tags" ctags-stamp)
endif()

find_program(ETAGS "etags")
if(NOT ETAGS MATCHES "ETAGS-NOTFOUND")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/TAGS"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
    COMMAND ${ETAGS} -o TAGS ${all_srcs} ${all_hdrs}
    COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
    DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_logging_code generate_config_h
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target(build_etags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TAGS" etags-stamp)
endif()

find_program(CSCOPE "cscope")
if(NOT CSCOPE MATCHES "CSCOPE-NOTFOUND")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.out"
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.in.out"
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/cscope.po.out"
    COMMAND ${CSCOPE} -b -q -R
    DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_logging_code generate_config_h
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target(build_cscope.out ALL DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cscope.out"
    "${CMAKE_CURRENT_SOURCE_DIR}/cscope.in.out"
    "${CMAKE_CURRENT_SOURCE_DIR}/cscope.po.out")
endif()

find_program(GTAGS "gtags")
if(NOT GTAGS MATCHES "GTAGS-NOTFOUND")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/GPATH"
    COMMAND ${GTAGS} --gtagsconf "${CMAKE_CURRENT_SOURCE_DIR}/.globalrc"
    DEPENDS ${all_srcs} ${all_hdrs} install_tdb_h generate_logging_code generate_config_h
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target(build_GTAGS ALL DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/GTAGS"
    "${CMAKE_CURRENT_SOURCE_DIR}/GRTAGS"
    "${CMAKE_CURRENT_SOURCE_DIR}/GPATH")
endif()
