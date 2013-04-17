if(BUILD_TESTING)
  file(GLOB srcs *.cc)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)

    foreach(impl lin tlog)
      ## each source file test_foo.cc creates binaries lt_test_foo.lin and
      ## lt_test_foo.tlog
      ## need lt_ prefix so that we have unique target names for cmake
      add_executable(lt_${base}.${impl} ${base})
      set_property(TARGET lt_${base}.${impl} APPEND PROPERTY
        COMPILE_DEFINITIONS "TESTDIR=\"dir.${base}.${impl}\"")
      add_common_options_to_binary_targets(lt_${base}.${impl})
      target_link_libraries(lt_${base}.${impl} lock_tree_${impl} range_tree_${impl} ft ${LIBTOKUPORTABILITY})
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.${impl}")

      add_test(lock_tree/${base}.${impl} lt_${base}.${impl})
    endforeach(impl)

    set_property(TARGET lt_${base}.tlog APPEND PROPERTY
      COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
  endforeach(src)

  ## run test_footprint_point_write.cc and test_footprint_range_write.cc in
  ## a bunch of different ways.
  foreach(impl lin tlog)
    foreach(type point range)
      foreach(i 1 2)
        add_test(lock_tree/check_footprint_${type}_${i}.${impl}
          lt_test_footprint_${type}_write.${impl} --nrows ${i})
      endforeach(i)
      foreach(i 10 100 1000)
        math(EXPR nlocks "${i} * 100")
        math(EXPR memsize "(${i} / 10) * 4096")
        add_test(lock_tree/check_footprint_${type}_${i}.${impl}
          lt_test_footprint_${type}_write.${impl} --nrows ${i} --max_locks ${nlocks} --max_lock_memory ${memsize})
      endforeach(i)
    endforeach(type)
  endforeach(impl)
endif(BUILD_TESTING)
