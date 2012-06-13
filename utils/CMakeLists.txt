set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

set(utils tokudb_gen tokudb_load tokudb_dump)
foreach(util ${utils})
  add_executable(${util} ${util}.c)
  set_target_properties(${util} PROPERTIES
    COMPILE_DEFINITIONS "IS_TDB=1;USE_TDB=1;TDB_IS_STATIC=0")
  add_space_separated_property(TARGET ${util} COMPILE_FLAGS "-fvisibility=hidden -fPIE")
  add_space_separated_property(TARGET ${util} LINK_FLAGS -pie)
  target_link_libraries(${util} ${LIBTOKUDB} ${LIBTOKUPORTABILITY})

  add_executable(${util}_static ${util}.c)
  set_target_properties(${util}_static PROPERTIES
    COMPILE_DEFINITIONS "IS_TDB=1;USE_TDB=1;TDB_IS_STATIC=1")
  add_space_separated_property(TARGET ${util}_static COMPILE_FLAGS "-fvisibility=hidden -fPIE")
  add_space_separated_property(TARGET ${util}_static LINK_FLAGS -pie)
  target_link_libraries(${util}_static ${LIBTOKUDB}_static ${ZLIB_LIBRARIES} ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} dl)
  set_targets_need_intel_libs(${util}_static)

  if(BDB_FOUND)
    add_executable(${util}.bdb ${util}.c)
    set_property(TARGET ${util}.bdb APPEND PROPERTY
      COMPILE_DEFINITIONS "IS_TDB=0;USE_BDB=1;TOKU_ALLOW_DEPRECATED")
    set_target_properties(${util}.bdb PROPERTIES
      INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../portability;${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(${util}.bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
    set_targets_need_intel_libs(${util}.bdb)
  endif()
endforeach(util)
