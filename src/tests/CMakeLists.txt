set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

if(BUILD_TESTING OR BUILD_SRC_TESTS)
  file(GLOB transparent_upgrade_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" upgrade*.cc)

  file(GLOB tdb_dontrun_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" bdb-simple-deadlock*.cc)
  string(REGEX REPLACE "\\.cc(;|$)" "\\1" tdb_dontrun_tests "${tdb_dontrun_srcs}")

  file(GLOB srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cc)
  list(REMOVE_ITEM srcs ${transparent_upgrade_srcs})

  set(recover_srcs test_log2.cc test_log3.cc test_log4.cc test_log5.cc test_log6.cc test_log7.cc test_log8.cc test_log9.cc test_log10.cc)
  file(GLOB abortrecover_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" recover-*.cc)
  file(GLOB loader_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" loader-*.cc)
  file(GLOB stress_test_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" test_stress*.cc)
  string(REGEX REPLACE "\\.cc(;|$)" ".recover\\1" recover_tests "${recover_srcs}")
  string(REGEX REPLACE "\\.cc(;|$)" ".abortrecover\\1" abortrecover_tests "${abortrecover_srcs}")
  string(REGEX REPLACE "\\.cc(;|$)" ".loader\\1" loader_tests "${loader_srcs}")
  string(REGEX REPLACE "\\.cc(;|$)" ".tdb\\1" stress_tests "${stress_test_srcs}")

  set(tdb_srcs ${srcs})
  list(REMOVE_ITEM tdb_srcs ${tdb_dontrun_srcs})
  string(REGEX REPLACE "\\.cc(;|$)" ".tdb\\1" tdb_bins "${tdb_srcs}")
  list(REMOVE_ITEM tdb_srcs ${abortrecover_srcs} ${loader_srcs})
  string(REGEX REPLACE "\\.cc(;|$)" ".tdb\\1" tdb_tests "${tdb_srcs}")

  if(BDB_FOUND)
    set(bdb_dontrun_srcs
      backwards_10_each_le_and_msg
      blackhole
      blocking-prelock-range
      blocking-set-range-reverse-0
      blocking-table-lock
      bug1381
      bug627
      cachetable-race
      checkpoint_1
      checkpoint_callback
      checkpoint_stress
      cursor-isolation
      cursor-set-del-rmw
      cursor-set-range-rmw
      db-put-simple-deadlock
      del-simple
      del-multiple
      del-multiple-huge-primary-row
      del-multiple-srcdb
      directory_lock
      diskfull
      env-put-multiple
      env_startup
      execute-updates
      filesize
      helgrind1
      helgrind2
      helgrind3
      hotindexer-bw
      hotindexer-error-callback
      hotindexer-insert-committed-optimized
      hotindexer-insert-committed
      hotindexer-insert-provisional
      hotindexer-lock-test
      hotindexer-multiclient
      hotindexer-nested-insert-committed
      hotindexer-put-abort
      hotindexer-put-commit
      hotindexer-put-multiple
      hotindexer-simple-abort
      hotindexer-simple-abort-put
      hotindexer-undo-do-test
      hotindexer-with-queries
      hot-optimize-table-tests
      insert-dup-prelock
      isolation
      isolation-read-committed
      keyrange
      keyrange-merge
      last-verify-time
      loader-cleanup-test
      loader-create-abort
      loader-create-close
      loader-dup-test
      loader-no-puts
      loader-reference-test
      loader-stress-del
      loader-stress-test
      loader-tpch-load
      lock-pressure
      manyfiles
      maxsize-for-loader
      multiprocess
      mvcc-create-table
      mvcc-many-committed
      perf_checkpoint_var
      perf_cursor_nop
      perf_insert
      perf_insert_multiple
      perf_malloc_free
      perf_nop
      perf_ptquery
      perf_ptquery2
      perf_read_write
      perf_txn_single_thread
      perf_xmalloc_free
      prelock-read-read
      prelock-read-write
      prelock-write-read
      prelock-write-write
      print_engine_status
      powerfail
      preload-db
      preload-db-nested
      progress
      put-multiple
      queries_with_deletes
      recover-2483
      recover-3113
      recover-5146
      recover-compare-db
      recover-compare-db-descriptor
      recover-del-multiple
      recover-del-multiple-abort
      recover-del-multiple-srcdb-fdelete-all
      recover-delboth-after-checkpoint
      recover-delboth-checkpoint
      recover-descriptor
      recover-descriptor2
      recover-descriptor3
      recover-descriptor4
      recover-descriptor5
      recover-descriptor6
      recover-descriptor7
      recover-descriptor8
      recover-descriptor9
      recover-descriptor10
      recover-descriptor11
      recover-descriptor12
      recover-fclose-in-checkpoint
      recover-fcreate-basementnodesize
      recover-flt1
      recover-flt2
      recover-flt3
      recover-flt4
      recover-flt5
      recover-flt6
      recover-flt7
      recover-flt8
      recover-flt9
      recover-flt10
      recover-hotindexer-simple-abort-put
      recover-loader-test
      recover-lsn-filter-multiple
      recover-put-multiple
      recover-put-multiple-abort
      recover-put-multiple-fdelete-all
      recover-put-multiple-fdelete-some
      recover-put-multiple-srcdb-fdelete-all
      recover-split-checkpoint
      recover-tablelock
      recover-test-logsuppress
      recover-test-logsuppress-put
      recover-test_stress1
      recover-test_stress2
      recover-test_stress3
      recover-test_stress_openclose
      recover-upgrade-db-descriptor-multihandle
      recover-upgrade-db-descriptor
      recover-update-multiple
      recover-update-multiple-abort
      recover-update_aborts
      recover-update_aborts_before_checkpoint
      recover-update_aborts_before_close
      recover-update_changes_values
      recover-update_changes_values_before_checkpoint
      recover-update_changes_values_before_close
      recover-update_broadcast_aborts
      recover-update_broadcast_aborts2
      recover-update_broadcast_aborts3
      recover-update_broadcast_aborts_before_checkpoint
      recover-update_broadcast_aborts_before_close
      recover-update_broadcast_changes_values
      recover-update_broadcast_changes_values2
      recover-update_broadcast_changes_values3
      recover-update_broadcast_changes_values_before_checkpoint
      recover-update_broadcast_changes_values_before_close
      recover-update_changes_values_before_close
      recovery_fileops_stress
      recovery_fileops_unit
      recovery_stress
      redirect
      replace-into-write-lock
      root_fifo_2
      root_fifo_32
      root_fifo_41
      shutdown-3344
      stat64
      stat64-create-modify-times
      stat64_flatten
      stat64-null-txn
      stat64-root-changes
      stress-gc
      stress-gc2
      test-xa-prepare
      test1324
      test1572
      test3219
      test3522
      test3522b
      test3529
      test_3645
      test_3529_insert_2
      test_3529_table_lock
      test_3755
      test_4015
      test_4368
      test_4657
      test_5015
      test_5469
      test-5138
      test938c
      test_abort1
      test_abort4
      test_abort5
      test_blobs_leaf_split
      test_bulk_fetch
      test_compression_methods
      test_cmp_descriptor
      test_db_change_pagesize
      test_db_change_xxx
      test_cursor_delete_2119
      test_db_descriptor
      test_db_descriptor_named_db
      test_db_txn_locks_read_uncommitted
      test_get_max_row_size
      test_large_update_broadcast_small_cachetable
      test_locktree_close
      test_logflush
      test_multiple_checkpoints_block_commit
      test_query
      test_redirect_func
      test_row_size_supported
      test_stress1
      test_stress2
      test_stress3
      test_stress4
      test_stress5
      test_stress6
      test_stress7
      test_stress_openclose
      test_stress_with_verify
      test_stress_hot_indexing
      test_transactional_descriptor
      test_trans_desc_during_chkpt
      test_trans_desc_during_chkpt2
      test_trans_desc_during_chkpt3
      test_trans_desc_during_chkpt4
      test_txn_abort6
      test_txn_abort8
      test_txn_abort9
      test_txn_close_open_commit
      test_txn_commit8
      test_txn_nested1
      test_txn_nested2
      test_txn_nested3
      test_txn_nested4
      test_txn_nested5
      test_update_abort_works
      test_update_calls_back
      test_update_can_delete_elements
      test_update_changes_values
      test_update_nonexistent_keys
      test_update_previously_deleted
      test_update_stress
      test_update_txn_snapshot_works_concurrently
      test_update_txn_snapshot_works_correctly_with_deletes
      test_update_broadcast_abort_works
      test_update_broadcast_calls_back
      test_update_broadcast_can_delete_elements
      test_update_broadcast_changes_values
      test_update_broadcast_previously_deleted
      test_update_broadcast_stress
      test_update_broadcast_update_fun_has_choices
      test_update_broadcast_with_empty_table
      test_update_broadcast_indexer
      test_update_broadcast_loader
      test_update_broadcast_nested_updates
      test_update_nested_updates
      test_update_with_empty_table
      txn-ignore
      transactional_fileops
      update-multiple-data-diagonal
      update-multiple-key0
      update-multiple-nochange
      update-multiple-with-indexer
      update
      upgrade_simple
      upgrade-test-1
      upgrade-test-2
      upgrade-test-3
      upgrade-test-4
      upgrade-test-5
      upgrade-test-6
      upgrade-test-7
      zombie_db
      )
    set(bdb_srcs ${srcs})
    string(REGEX REPLACE "\\.cc(;|$)" "\\1" bdb_testbases "${bdb_srcs}")
    list(REMOVE_ITEM bdb_testbases ${bdb_dontrun_srcs})
    string(REGEX REPLACE "(.)(;|$)" "\\1.bdb\\2" bdb_tests "${bdb_testbases}")
    set(bdb_bins ${bdb_tests})
  endif()

  set(tdb_tests_that_should_fail
    test_db_no_env.tdb
    test_log8.recover
    test_log9.recover
    test_log10.recover
    recover-missing-dbfile.abortrecover
    recover-missing-dbfile-2.abortrecover
    recover-dbopen-eclose.abortrecover
    loader-tpch-load.loader
    )

  ## #5138 only reproduces when using the static library.
  list(REMOVE_ITEM tdb_bins test-5138.tdb)
  add_executable(test-5138.tdb test-5138)
  target_link_libraries(test-5138.tdb ${LIBTOKUDB}_static ${LIBTOKUPORTABILITY}_static)
  set_property(TARGET test-5138.tdb APPEND PROPERTY
    COMPILE_DEFINITIONS "ENVDIR=\"dir.test-5138.tdb\";USE_TDB;IS_TDB=1;TOKUDB=1")
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.test-5138.tdb")
  add_common_options_to_binary_targets(test-5138.tdb)
  add_test(ydb/test-5138.tdb test-5138.tdb)

  foreach(bin ${tdb_bins})
    get_filename_component(base ${bin} NAME_WE)

    add_executable(${base}.tdb ${base})
    target_link_libraries(${base}.tdb ${LIBTOKUDB} ${LIBTOKUPORTABILITY})
    set_property(TARGET ${base}.tdb APPEND PROPERTY
      COMPILE_DEFINITIONS "ENVDIR=\"dir.${bin}\";USE_TDB;IS_TDB=1;TOKUDB=1")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${bin}")
    add_common_options_to_binary_targets(${base}.tdb)
  endforeach(bin)

  if(BDB_FOUND)
    foreach(bin ${bdb_bins})
      get_filename_component(base ${bin} NAME_WE)

      add_executable(${base}.bdb ${base})
      set_property(TARGET ${base}.bdb APPEND PROPERTY
        COMPILE_DEFINITIONS "ENVDIR=\"dir.${bin}\";USE_BDB;IS_TDB=0;TOKU_ALLOW_DEPRECATED")
      set_target_properties(${base}.bdb PROPERTIES
        INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../portability;${CMAKE_CURRENT_SOURCE_DIR}/../..")
      target_link_libraries(${base}.bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${bin}")
      add_common_options_to_binary_targets(${base}.bdb)
    endforeach(bin)
  endif()

  foreach(bin loader-cleanup-test.tdb diskfull.tdb)
    set_property(TARGET ${bin} APPEND PROPERTY
      COMPILE_DEFINITIONS DONT_DEPRECATE_WRITES)
  endforeach(bin)

  macro(declare_custom_tests)
    foreach(test ${ARGN})
      list(REMOVE_ITEM tdb_tests ${test})
    endforeach(test)
  endmacro(declare_custom_tests)

  function(add_custom_executable prefix binary source)
    add_executable(${prefix}_${binary} ${source})
    target_link_libraries(${prefix}_${binary} ${LIBTOKUDB} ${LIBTOKUPORTABILITY})
    set_target_properties(${prefix}_${binary} PROPERTIES
      COMPILE_DEFINITIONS "ENVDIR=\"dir.${prefix}_${source}.tdb\";USE_TDB;IS_TDB=1;TOKUDB=1")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${prefix}_${source}.tdb")
    add_common_options_to_binary_targets(${prefix}_${binary})
  endfunction(add_custom_executable)

  declare_custom_tests(test1426.tdb)
  if(BDB_FOUND)
    macro(declare_custom_bdb_tests)
      foreach(test ${ARGN})
        list(REMOVE_ITEM bdb_tests ${test})
      endforeach(test)
    endmacro(declare_custom_bdb_tests)

    declare_custom_bdb_tests(test1426.bdb)
    configure_file(run_test1426.sh . COPYONLY)
    add_test(NAME ydb/test1426.tdb
      COMMAND run_test1426.sh
          $<TARGET_FILE:test1426.tdb> $<TARGET_FILE:test1426.bdb>
          $<TARGET_FILE:tokudb_dump> "${BDB_INCLUDE_DIR}/../bin/db_dump")
    add_dependencies(test1426.tdb tokudb_dump)
  endif()

  string(REGEX REPLACE "\\.cc(;|$)" ".tdb\\1" recover_would_be_tdb_tests "${recover_srcs}")
  declare_custom_tests(${recover_would_be_tdb_tests})

  declare_custom_tests(powerfail.tdb)
  add_test(ydb/powerfail.tdb echo must run powerfail by hand)

  declare_custom_tests(checkpoint_stress.tdb)
  configure_file(run_checkpoint_stress_test.sh . COPYONLY)
  add_test(NAME ydb/checkpoint_stress.tdb
    COMMAND run_checkpoint_stress_test.sh $<TARGET_FILE:checkpoint_stress.tdb> 5 5001 137)

  declare_custom_tests(recover_stress.tdb)
  add_custom_executable(recover stress.tdb checkpoint_stress)
  configure_file(run_recover_stress_test.sh . COPYONLY)
  add_test(NAME ydb/recover_stress.tdb
    COMMAND run_recover_stress_test.sh $<TARGET_FILE:recover_stress.tdb> 5 5001 137)

  declare_custom_tests(diskfull.tdb)
  configure_file(run_diskfull_test.sh . COPYONLY)
  add_test(NAME ydb/diskfull.tdb
    COMMAND run_diskfull_test.sh $<TARGET_FILE:diskfull.tdb> 134)

  declare_custom_tests(recovery_fileops_unit.tdb)
  configure_file(run_recovery_fileops_unit_test.sh . COPYONLY)
  add_test(NAME ydb/recovery_fileops_unit.tdb
    COMMAND run_recovery_fileops_unit_test.sh $<TARGET_FILE:recovery_fileops_unit.tdb> 137)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.recovery_fileops_unit.cc.errors")

  if (NOT (CMAKE_SYSTEM_NAME MATCHES Darwin OR
           (CMAKE_CXX_COMPILER_ID STREQUAL Intel AND
            CMAKE_BUILD_TYPE STREQUAL Release)
           OR USE_GCOV))
    declare_custom_tests(helgrind1.tdb)
    add_test(NAME ydb/helgrind_helgrind1.tdb
      COMMAND valgrind --quiet --tool=helgrind --error-exitcode=1 --log-file=helgrind1.tdb.deleteme $<TARGET_FILE:helgrind1.tdb>)
    set_tests_properties(ydb/helgrind_helgrind1.tdb PROPERTIES WILL_FAIL TRUE)
  endif()
  declare_custom_tests(helgrind2.tdb)
  declare_custom_tests(helgrind3.tdb)
  add_helgrind_test(ydb/helgrind_helgrind2.tdb $<TARGET_FILE:helgrind2.tdb>)
  add_helgrind_test(ydb/helgrind_helgrind3.tdb $<TARGET_FILE:helgrind3.tdb>)

  declare_custom_tests(test_groupcommit_count.tdb)
  add_test(ydb/test_groupcommit_count.tdb test_groupcommit_count.tdb -n 1 -p vgrind)

  add_custom_executable(helgrind test_groupcommit_count.tdb test_groupcommit_count)
  add_helgrind_test(ydb/helgrind_test_groupcommit_count.tdb $<TARGET_FILE:helgrind_test_groupcommit_count.tdb> -n 1 -p hgrind)

  add_custom_executable(drd test_groupcommit_count.tdb test_groupcommit_count)
  add_drd_test(ydb/drd_test_groupcommit_count.tdb $<TARGET_FILE:drd_test_groupcommit_count.tdb> -n 1 -p drd)

  add_custom_executable(drd test_4015.tdb test_4015)
  add_drd_test(ydb/drd_test_4015.tdb $<TARGET_FILE:drd_test_4015.tdb>)

  configure_file(run_stress_test.sh . COPYONLY)
  foreach(src ${stress_test_srcs})
    if(NOT ${src} MATCHES hot_index)
      get_filename_component(base ${src} NAME_WE)
      set(test ${base}.tdb)

      add_test(ydb/${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:${test}> 150000 1000)

      add_custom_executable(tiny ${test} ${base})
      add_drd_test(ydb/drd_tiny_${test} $<TARGET_FILE:tiny_${test}> --num_seconds 5 --num_elements 150)
      add_custom_executable(mid ${test} ${base})
      add_drd_test(ydb/drd_mid_${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:mid_${test}> 10000 100)
      set_tests_properties(ydb/drd_mid_${test} PROPERTIES TIMEOUT 5400)
      add_custom_executable(large ${test} ${base})
      add_drd_test(ydb/drd_large_${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:large_${test}> 150000 1000)
      set_tests_properties(ydb/drd_large_${test} PROPERTIES TIMEOUT 14400)
    endif()
  endforeach(src)

  configure_file(run_upgrade_stress_test.sh . COPYONLY)
  foreach(src ${stress_test_srcs})
    if (NOT ${src} MATCHES test_stress_openclose)
      get_filename_component(base ${src} NAME_WE)
      set(test ${base}.tdb)

      foreach(oldver 4.2.0 5.0.8 5.2.7 6.0.0 6.1.0)
        foreach(p_or_s pristine stressed)
          if (NOT (${base} MATCHES test_stress4 AND ${p_or_s} MATCHES stressed))
            foreach(size 2000)
              add_custom_executable(upgrade_${oldver}_${p_or_s}_${size} ${test} ${base})
              add_test(
                NAME ydb/${test}/upgrade/${oldver}/${p_or_s}/${size}
                COMMAND ${CMAKE_CFG_INTDIR}/run_upgrade_stress_test.sh $<TARGET_FILE:upgrade_${oldver}_${p_or_s}_${size}_${test}> ${TOKU_SVNROOT}/tokudb.data/old-stress-test-envs/${oldver}/saved${p_or_s}-${size}-dir dir.upgrade_${oldver}_${p_or_s}_${size}_${base}.tdb ${size} 600)
              set_tests_properties(ydb/${test}/upgrade/${oldver}/${p_or_s}/${size} PROPERTIES TIMEOUT 5400)
            endforeach(size)
          endif ()
        endforeach(p_or_s)
      endforeach(oldver)
    endif ()
  endforeach(src)

  ## for some reason this rule doesn't run with the makefile and it crashes with this rule, so I'm disabling this special case
  #declare_custom_tests(test_thread_stack.tdb)
  #add_custom_command(OUTPUT run_test_thread_stack.sh
  #  COMMAND install "${CMAKE_CURRENT_SOURCE_DIR}/run_test_thread_stack.sh" "${CMAKE_CFG_INTDIR}"
  #  MAIN_DEPENDENCY run_test_thread_stack.sh
  #  VERBATIM)
  #add_custom_target(install_run_test_thread_stack.sh ALL DEPENDS run_test_thread_stack.sh)
  #add_test(ydb/test_thread_stack.tdb run_test_thread_stack.sh "${CMAKE_CFG_INTDIR}/test_thread_stack.tdb")

  declare_custom_tests(root_fifo_41.tdb)
  foreach(num RANGE 1 100)
    add_test(ydb/root_fifo_41_${num}_populate.tdb root_fifo_41.tdb -n ${num} -h root_fifo_41_${num}_populate.tdbdir -populate)
    add_test(ydb/root_fifo_41_${num}_nopopulate.tdb root_fifo_41.tdb -n ${num} -h root_fifo_41_${num}_nopopulate.tdbdir)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
      root_fifo_41_${num}_populate.tdbdir
      root_fifo_41_${num}_nopopulate.tdbdir
      )
  endforeach(num)

  declare_custom_tests(test3039.tdb)
  configure_file(run_test3039.sh . COPYONLY)
  add_test(NAME ydb/test3039.tdb
    COMMAND run_test3039.sh $<TARGET_FILE:test3039.tdb>)

  declare_custom_tests(test_abort4.tdb)
  foreach(num RANGE -1 19)
    add_test(ydb/test_abort4_${num}_0.tdb test_abort4.tdb -e dir.test_abort4_${num}_0.tdb -c 0 -l ${num})
    add_test(ydb/test_abort4_${num}_1.tdb test_abort4.tdb -e dir.test_abort4_${num}_1.tdb -c 1 -l ${num})
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES dir.test_abort4_${num}_0.tdb dir.test_abort4_${num}_1.tdb)
  endforeach(num)

  set_property(SOURCE loader-stress-test APPEND PROPERTY COMPILE_DEFINITIONS
    "OLDDATADIR=\"${TOKU_SVNROOT}/tokudb.data/\"")
  list(REMOVE_ITEM loader_tests loader-stress-test.loader)
  add_test(ydb/loader-stress-test0.tdb loader-stress-test.tdb -c -e dir.loader-stress-test0.tdb)
  add_test(ydb/loader-stress-test1.tdb loader-stress-test.tdb -c -p -e dir.loader-stress-test1.tdb)
  add_test(ydb/loader-stress-test2.tdb loader-stress-test.tdb -r 5000 -s -e dir.loader-stress-test2.tdb)
  add_test(ydb/loader-stress-test3.tdb loader-stress-test.tdb -u -c -e dir.loader-stress-test3.tdb)
  add_test(ydb/loader-stress-test4.tdb loader-stress-test.tdb -r 10000000 -c -e dir.loader-stress-test4.tdb)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.loader-stress-test0.tdb
    dir.loader-stress-test1.tdb
    dir.loader-stress-test2.tdb
    dir.loader-stress-test3.tdb
    dir.loader-stress-test4.tdb
    )

  list(REMOVE_ITEM loader_tests loader-dup-test.loader)
  add_test(ydb/loader-dup-test0.tdb loader-dup-test.tdb -e dir.loader-dup-test0.tdb)
  add_test(ydb/loader-dup-test1.tdb loader-dup-test.tdb -d 1 -r 500000 -e dir.loader-dup-test1.tdb)
  add_test(ydb/loader-dup-test2.tdb loader-dup-test.tdb -d 1 -r 1000000 -e dir.loader-dup-test2.tdb)
  add_test(ydb/loader-dup-test3.tdb loader-dup-test.tdb -d 1 -s -r 100 -e dir.loader-dup-test3.tdb)
  add_test(ydb/loader-dup-test4.tdb loader-dup-test.tdb -d 1 -s -r 1000 -e dir.loader-dup-test4.tdb)
  add_test(ydb/loader-dup-test5.tdb loader-dup-test.tdb -d 1 -s -r 1000 -E -e dir.loader-dup-test5.tdb)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.loader-dup-test0.tdb
    dir.loader-dup-test1.tdb
    dir.loader-dup-test2.tdb
    dir.loader-dup-test3.tdb
    dir.loader-dup-test4.tdb
    dir.loader-dup-test5.tdb
    )

  ## as part of #4503, we took out test 1 and 3
  list(REMOVE_ITEM loader_tests loader-cleanup-test.loader)
  add_test(ydb/loader-cleanup-test0.tdb loader-cleanup-test.tdb -s -r 800 -e dir.loader-cleanup-test0.tdb)
  #add_test(ydb/loader-cleanup-test1.tdb loader-cleanup-test.tdb -s -r 800 -p -e dir.loader-cleanup-test1.tdb)
  add_test(ydb/loader-cleanup-test2.tdb loader-cleanup-test.tdb -s -r 8000 -e dir.loader-cleanup-test2.tdb)
  #add_test(ydb/loader-cleanup-test3.tdb loader-cleanup-test.tdb -s -r 8000 -p -e dir.loader-cleanup-test3.tdb)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.loader-cleanup-test0.tdb
    dir.loader-cleanup-test1.tdb
    dir.loader-cleanup-test2.tdb
    dir.loader-cleanup-test3.tdb
    )

  declare_custom_tests(keyrange.tdb)
  add_test(ydb/keyrange-get0.tdb keyrange.tdb --get 0 --envdir dir.keyrange-get0.tdb)
  add_test(ydb/keyrange-get1.tdb keyrange.tdb --get 1 --envdir dir.keyrange-get1.tdb)
  add_test(ydb/keyrange-random-get0.tdb keyrange.tdb --get 0 --random_keys 1 --envdir dir.keyrange-random-get0.tdb)
  add_test(ydb/keyrange-random-get1.tdb keyrange.tdb --get 1 --random_keys 1 --envdir dir.keyrange-random-get1.tdb)
  add_test(ydb/keyrange-loader-get0.tdb keyrange.tdb --get 0 --loader 1 --envdir dir.keyrange-loader-get0.tdb)
  add_test(ydb/keyrange-loader-get1.tdb keyrange.tdb --get 1 --loader 1 --envdir dir.keyrange-loader-get1.tdb)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.keyrange-get0.tdb
    dir.keyrange-get1.tdb
    dir.keyrange-random-get0.tdb
    dir.keyrange-random-get1.tdb
    dir.keyrange-loader-get0.tdb
    dir.keyrange-loader-get1.tdb
    )

  declare_custom_tests(maxsize-for-loader.tdb)
  add_test(ydb/maxsize-for-loader-A.tdb maxsize-for-loader.tdb -e dir.maxsize-for-loader-A.tdb -f)
  add_test(ydb/maxsize-for-loader-B.tdb maxsize-for-loader.tdb -e dir.maxsize-for-loader-B.tdb)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.maxsize-for-loader-A.tdb
    dir.maxsize-for-loader-B.tdb
    )

  declare_custom_tests(hotindexer-undo-do-test.tdb)
  file(GLOB hotindexer_tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "hotindexer-undo-do-tests/*.test")
  file(GLOB hotindexer_results RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "hotindexer-undo-do-tests/*.result")
  configure_file(run-hotindexer-undo-do-tests.bash . COPYONLY)
  foreach(result ${hotindexer_results})
    configure_file(${result} ${result} COPYONLY)
  endforeach(result)
  foreach(test ${hotindexer_tests})
    configure_file(${test} ${test} COPYONLY)
    add_test(NAME ydb/${test} COMMAND run-hotindexer-undo-do-tests.bash ${test})
  endforeach()

  foreach(test ${tdb_tests} ${bdb_tests})
    add_test(ydb/${test} ${test})
  endforeach(test)

  configure_file(run_recover_test.sh . COPYONLY)
  foreach(recover_test ${recover_tests})
    get_filename_component(base ${recover_test} NAME_WE)
    add_test(NAME ydb/${recover_test}
      COMMAND run_recover_test.sh $<TARGET_FILE:${base}.tdb> "dir.${base}.tdb" $<TARGET_FILE:tdb-recover> $<TARGET_FILE:tokudb_dump_static>)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.tdb" "dir.${base}.tdb.recover")
  endforeach(recover_test)

  configure_file(run_abortrecover_test.sh . COPYONLY)
  foreach(abortrecover_test ${abortrecover_tests})
    get_filename_component(base ${abortrecover_test} NAME_WE)
    add_test(NAME ydb/${base}.abortrecover
      COMMAND run_abortrecover_test.sh $<TARGET_FILE:${base}.tdb> 137)
  endforeach(abortrecover_test)

  foreach(loader_test ${loader_tests})
    get_filename_component(base ${loader_test} NAME_WE)
    add_test(ydb/${base}.nop.loader ${base}.tdb -e "dir.${base}.nop.loader")
    add_test(ydb/${base}.p.loader ${base}.tdb -p -e "dir.${base}.p.loader")
    if("${tdb_tests_that_should_fail}" MATCHES "${base}.loader")
      list(REMOVE_ITEM tdb_tests_that_should_fail ${base}.loader)
      list(APPEND tdb_tests_that_should_fail ${base}.nop.loader ${base}.p.loader)
    endif()
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.nop.loader" "dir.${base}.p.loader")
  endforeach(loader_test)

  set(tdb_tests_that_should_fail "ydb/${tdb_tests_that_should_fail}")
  string(REGEX REPLACE ";" ";ydb/" tdb_tests_that_should_fail "${tdb_tests_that_should_fail}")
  set_tests_properties(${tdb_tests_that_should_fail} PROPERTIES WILL_FAIL TRUE)

  ## give some tests, that time out normally, 1 hour to complete
  set(long_tests
    ydb/checkpoint_1.tdb
    ydb/drd_test_groupcommit_count.tdb
    ydb/env-put-multiple.tdb
    ydb/hot-optimize-table-tests.tdb
    ydb/loader-cleanup-test0.tdb
    ydb/loader-cleanup-test2.tdb
    ydb/manyfiles.tdb
    ydb/recovery_fileops_stress.tdb
    ydb/root_fifo_1.tdb
    ydb/root_fifo_2.tdb
    ydb/root_fifo_31.tdb
    ydb/root_fifo_32.tdb
    ydb/shutdown-3344.tdb
    ydb/stat64-create-modify-times.tdb
    ydb/test1572.tdb
    ydb/test3529.tdb
    ydb/test_abort4_19_0.tdb
    ydb/test_abort4_19_1.tdb
    ydb/test_abort5.tdb
    ydb/test_archive1.tdb
    ydb/test_logmax.tdb
    ydb/test_query.tdb
    ydb/test_txn_abort5.tdb
    ydb/test_txn_abort5a.tdb
    ydb/test_txn_abort6.tdb
    ydb/test_txn_nested2.tdb
    ydb/test_txn_nested4.tdb
    ydb/test_txn_nested5.tdb
    ydb/test_update_broadcast_stress.tdb
    ydb/test_update_stress.tdb
    )
  if (BDB_FOUND)
    list(APPEND long_tests
      ydb/root_fifo_1.bdb
      ydb/root_fifo_31.bdb
      ydb/rowsize.bdb
      ydb/test_log10.bdb
      ydb/test_log7.bdb
      ydb/test_logmax.bdb
      )
  endif (BDB_FOUND)
  set_tests_properties(${long_tests} PROPERTIES TIMEOUT 3600)
  ## some take even longer, with valgrind
  set(extra_long_tests
    ydb/drd_test_4015.tdb
    ydb/hotindexer-with-queries.tdb
    ydb/loader-dup-test0.tdb
    ydb/loader-stress-del.nop.loader
    ydb/loader-stress-del.p.loader
    ydb/loader-stress-test4.tdb
    ydb/test3039.tdb
    )
  if (BDB_FOUND)
    list(APPEND extra_long_tests
      ydb/test_groupcommit_count.bdb
      )
  endif (BDB_FOUND)
  set_tests_properties(${extra_long_tests} PROPERTIES TIMEOUT 7200)
  ## these really take a long time with valgrind
  set(phenomenally_long_tests
    ydb/checkpoint_stress.tdb
    ydb/recover_stress.tdb
    ydb/recovery_fileops_unit.tdb
    )
  if (BDB_FOUND)
    list(APPEND phenomenally_long_tests
      ydb/test1426.tdb
      )
  endif (BDB_FOUND)
  set_tests_properties(${phenomenally_long_tests} PROPERTIES TIMEOUT 14400)
endif(BUILD_TESTING OR BUILD_SRC_TESTS)
