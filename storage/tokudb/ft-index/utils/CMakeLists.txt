set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

set(utils tokudb_gen tokudb_load tokudb_dump)
foreach(util ${utils})
  add_executable(${util} ${util}.cc)
  set_target_properties(${util} PROPERTIES
    COMPILE_DEFINITIONS "IS_TDB=1;USE_TDB=1;TDB_IS_STATIC=1")
  target_link_libraries(${util} ${LIBTOKUDB}_static ft_static ${ZLIB_LIBRARY} lzma ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

  add_space_separated_property(TARGET ${util} COMPILE_FLAGS -fvisibility=hidden)

  if(BDB_FOUND)
    add_executable(${util}.bdb ${util})
    set_property(TARGET ${util}.bdb APPEND PROPERTY
      COMPILE_DEFINITIONS "IS_TDB=0;USE_BDB=1;TOKU_ALLOW_DEPRECATED")
    set_target_properties(${util}.bdb PROPERTIES
      INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../portability;${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(${util}.bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
    add_space_separated_property(TARGET ${util}.bdb COMPILE_FLAGS -fvisibility=hidden)
  endif()
endforeach(util)
