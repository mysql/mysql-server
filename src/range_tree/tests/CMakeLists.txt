if(BUILD_TESTING)
  file(GLOB srcs *.c)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)

    foreach(impl lin tlog)
      ## each source file test_foo.c creates binaries rt_test_foo.lin and
      ## rt_test_foo.tlog
      add_executable(rt_${base}.${impl} ${src})
      set_property(TARGET rt_${base}.${impl} APPEND PROPERTY
        COMPILE_DEFINITIONS "TESTDIR=\"dir.${base}.c.${impl}\"")
      add_space_separated_property(TARGET rt_${base}.${impl} COMPILE_FLAGS -fvisibility=hidden)
      target_link_libraries(rt_${base}.${impl}
        range_tree_${impl}_static
        ft
        ${ZLIB_LIBRARIES}
        ${LIBTOKUPORTABILITY}
        )
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.c.${impl}")

      add_test(rt_${base}.${impl} rt_${base}.${impl})
    endforeach(impl)

    set_property(TARGET rt_${base}.tlog APPEND PROPERTY
      COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
  endforeach(src)
endif(BUILD_TESTING)