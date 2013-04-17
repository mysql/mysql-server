if(BUILD_TESTING)
  file(GLOB srcs *.c)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)

    foreach(impl lin tlog)
      ## each source file test_foo.c creates binaries rt_test_foo.lin and
      ## rt_test_foo.tlog
      ## need rt_ prefix so that we have unique target names for cmake
      add_executable(rt_${base}.${impl} ${src})
      set_property(TARGET rt_${base}.${impl} APPEND PROPERTY
        COMPILE_DEFINITIONS "TESTDIR=\"dir.${base}.c.${impl}\"")
      add_common_options_to_binary_targets(rt_${base}.${impl})
      target_link_libraries(rt_${base}.${impl} range_tree_${impl} ${LIBTOKUPORTABILITY})
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.c.${impl}")

      add_test(range_tree/${base}.${impl} rt_${base}.${impl})
    endforeach(impl)

    set_property(TARGET rt_${base}.tlog APPEND PROPERTY
      COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
  endforeach(src)
endif(BUILD_TESTING)
