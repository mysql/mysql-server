if(BUILD_TESTING)
  file(GLOB srcs *.cc)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)

    ## each source file test_foo.cc creates lt_test_foo
    ## need lt_ prefix so that we have unique target names for cmake
    add_executable(lt_${base} ${base})
    add_common_options_to_binary_targets(lt_${base})
    target_link_libraries(lt_${base} locktree ft ${LIBTOKUPORTABILITY})
    add_test(locktree/${base} lt_${base})

  endforeach(src)
endif(BUILD_TESTING)
