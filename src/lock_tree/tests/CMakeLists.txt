if(BUILD_TESTING)
  file(GLOB srcs *.c)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)

    foreach(impl lin tlog)
      add_executable(lt_${base}.${impl} ${src})
      set_property(TARGET lt_${base}.${impl} APPEND PROPERTY
        COMPILE_DEFINITIONS "TESTDIR=\"dir.${base}.c.${impl}\"")
      target_link_libraries(lt_${base}.${impl}
        lock_tree_${impl}_static
        range_tree_${impl}_static
        newbrt
        ${LIBTOKUPORTABILITY}
        )
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.c.${impl}")

      add_test(lt_${base}.${impl} lt_${base}.${impl})
    endforeach(impl)

    set_property(TARGET lt_${base}.tlog APPEND PROPERTY
      COMPILE_DEFINITIONS TOKU_RT_NOOVERLAPS)
  endforeach(src)

  foreach(impl lin tlog)
    foreach(type point range)
      foreach(i 1 2)
        add_test(check_footprint_${type}_${i}.${impl}
          lt_test_footprint_${type}_write.${impl} --nrows ${i})
      endforeach(i)
      foreach(i 10 100 1000)
        math(EXPR nlocks "${i} * 100")
        math(EXPR memsize "(${i} / 10) * 4096")
        add_test(check_footprint_${type}_${i}.${impl}
          lt_test_footprint_${type}_write.${impl} --nrows ${i} --max_locks ${nlocks} --max_lock_memory ${memsize})
      endforeach(i)
    endforeach(type)
  endforeach(impl)
endif(BUILD_TESTING)