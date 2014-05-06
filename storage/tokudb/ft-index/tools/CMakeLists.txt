set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

set(utils tokudb_gen tokudb_load tokudb_dump)
foreach(util ${utils})
  add_executable(${util} ${util}.cc)
  set_target_properties(${util} PROPERTIES
    COMPILE_DEFINITIONS "IS_TDB=1;USE_TDB=1;TDB_IS_STATIC=1")
  target_link_libraries(${util} ${LIBTOKUDB}_static ft_static z lzma ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

  add_space_separated_property(TARGET ${util} COMPILE_FLAGS -fvisibility=hidden)
endforeach(util)
