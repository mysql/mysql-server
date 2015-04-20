set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

set(tools tokudb_dump tokuftdump tdb_logprint tdb-recover ftverify ba_replay)
foreach(tool ${tools})
  add_executable(${tool} ${tool}.cc)
  add_dependencies(${tool} install_tdb_h)
  target_link_libraries(${tool} ${LIBTOKUDB}_static ft_static z lzma ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})

  add_space_separated_property(TARGET ${tool} COMPILE_FLAGS -fvisibility=hidden)
endforeach(tool)

# link in math.h library just for this tool.
target_link_libraries(ftverify m)

install(
  TARGETS tokuftdump
  DESTINATION ${INSTALL_BINDIR}
  COMPONENT Server
  )

