set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

if(BUILD_TESTING)
  file(GLOB transparent_upgrade_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" upgrade*.c)

  file(GLOB tdb_dontrun_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" bdb-simple-deadlock*.c)
  string(REGEX REPLACE "\\.c(;|$)" "\\1" tdb_dontrun_tests "${tdb_dontrun_srcs}")

  file(GLOB srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.c)
  list(REMOVE_ITEM srcs ${transparent_upgrade_srcs})

  set(recover_srcs test_log2.c test_log3.c test_log4.c test_log5.c test_log6.c test_log7.c test_log8.c test_log9.c test_log10.c)
  file(GLOB abortrecover_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" recover-*.c)
  file(GLOB loader_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" loader-*.c)
  file(GLOB stress_test_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" test_stress*.c)
  string(REGEX REPLACE "\\.c(;|$)" ".recover\\1" recover_tests "${recover_srcs}")
  string(REGEX REPLACE "\\.c(;|$)" ".abortrecover\\1" abortrecover_tests "${abortrecover_srcs}")
  string(REGEX REPLACE "\\.c(;|$)" ".loader\\1" loader_tests "${loader_srcs}")
  string(REGEX REPLACE "\\.c(;|$)" ".tdb\\1" stress_tests "${stress_test_srcs}")

  set(tdb_srcs ${srcs})
  list(REMOVE_ITEM tdb_srcs ${tdb_dontrun_srcs})
  string(REGEX REPLACE "\\.c(;|$)" ".tdb\\1" tdb_bins "${tdb_srcs}")
  list(REMOVE_ITEM tdb_srcs ${abortrecover_srcs} ${loader_srcs})
  string(REGEX REPLACE "\\.c(;|$)" ".tdb\\1" tdb_tests "${tdb_srcs}")

  if(BDB_FOUND)
    set(bdb_dontrun_srcs
      backwards_10_each_le_and_msg.c
      blocking-prelock-range.c
      blocking-set-range-reverse-0.c
      blocking-table-lock.c
      bug1381.c
      bug627.c
      cachetable-race.c
      checkpoint_1.c
      checkpoint_callback.c
      checkpoint_stress.c
      cursor-isolation.c
      cursor-set-del-rmw.c
      cursor-set-range-rmw.c
      db-put-simple-deadlock.c
      del-simple.c
      del-multiple.c
      del-multiple-huge-primary-row.c
      del-multiple-srcdb.c
      directory_lock.c
      diskfull.c
      env-put-multiple.c
      env_startup.c
      execute-updates.c
      filesize.c
      helgrind1.c
      helgrind2.c
      helgrind3.c
      hotindexer-bw.c
      hotindexer-error-callback.c
      hotindexer-insert-committed-optimized.c
      hotindexer-insert-committed.c
      hotindexer-insert-provisional.c
      hotindexer-lock-test.c
      hotindexer-multiclient.c
      hotindexer-nested-insert-committed.c
      hotindexer-put-abort.c
      hotindexer-put-commit.c
      hotindexer-put-multiple.c
      hotindexer-simple-abort.c
      hotindexer-simple-abort-put.c
      hotindexer-undo-do-test.c
      hotindexer-with-queries.c
      hot-optimize-table-tests.c
      insert-dup-prelock.c
      isolation.c
      isolation-read-committed.c
      keyrange.c
      keyrange-merge.c
      last-verify-time.c
      loader-cleanup-test.c
      loader-create-abort.c
      loader-create-close.c
      loader-dup-test.c
      loader-no-puts.c
      loader-reference-test.c
      loader-stress-del.c
      loader-stress-test.c
      loader-tpch-load.c
      lock-pressure.c
      manyfiles.c
      maxsize-for-loader.c
      multiprocess.c
      mvcc-create-table.c
      mvcc-many-committed.c
      perf_checkpoint_var.c
      perf_cursor_nop.c
      perf_insert.c
      perf_insert_multiple.c
      perf_malloc_free.c
      perf_nop.c
      perf_ptquery.c
      perf_ptquery2.c
      perf_xmalloc_free.c
      prelock-read-read.c
      prelock-read-write.c
      prelock-write-read.c
      prelock-write-write.c
      print_engine_status.c
      powerfail.c
      preload-db.c
      preload-db-nested.c
      progress.c
      put-multiple.c
      queries_with_deletes.c
      recover-2483.c
      recover-3113.c
      recover-5146.c
      recover-compare-db.c
      recover-compare-db-descriptor.c
      recover-del-multiple.c
      recover-del-multiple-abort.c
      recover-del-multiple-srcdb-fdelete-all.c
      recover-delboth-after-checkpoint.c
      recover-delboth-checkpoint.c
      recover-descriptor.c
      recover-descriptor2.c
      recover-descriptor3.c
      recover-descriptor4.c
      recover-descriptor5.c
      recover-descriptor6.c
      recover-descriptor7.c
      recover-descriptor8.c
      recover-descriptor9.c
      recover-descriptor10.c
      recover-descriptor11.c
      recover-descriptor12.c
      recover-fclose-in-checkpoint.c
      recover-fcreate-basementnodesize.c
      recover-flt1.c
      recover-flt2.c
      recover-flt3.c
      recover-flt4.c
      recover-flt5.c
      recover-flt6.c
      recover-flt7.c
      recover-flt8.c
      recover-flt9.c
      recover-flt10.c
      recover-hotindexer-simple-abort-put.c
      recover-loader-test.c
      recover-lsn-filter-multiple.c
      recover-put-multiple.c
      recover-put-multiple-abort.c
      recover-put-multiple-fdelete-all.c
      recover-put-multiple-fdelete-some.c
      recover-put-multiple-srcdb-fdelete-all.c
      recover-split-checkpoint.c
      recover-tablelock.c
      recover-test-logsuppress.c
      recover-test-logsuppress-put.c
      recover-test_stress1.c
      recover-test_stress2.c
      recover-test_stress3.c
      recover-upgrade-db-descriptor-multihandle.c
      recover-upgrade-db-descriptor.c
      recover-update-multiple.c
      recover-update-multiple-abort.c
      recover-update_aborts.c
      recover-update_aborts_before_checkpoint.c
      recover-update_aborts_before_close.c
      recover-update_changes_values.c
      recover-update_changes_values_before_checkpoint.c
      recover-update_changes_values_before_close.c
      recover-update_broadcast_aborts.c
      recover-update_broadcast_aborts2.c
      recover-update_broadcast_aborts3.c
      recover-update_broadcast_aborts_before_checkpoint.c
      recover-update_broadcast_aborts_before_close.c
      recover-update_broadcast_changes_values.c
      recover-update_broadcast_changes_values2.c
      recover-update_broadcast_changes_values3.c
      recover-update_broadcast_changes_values_before_checkpoint.c
      recover-update_broadcast_changes_values_before_close.c
      recover-update_changes_values_before_close.c
      recovery_fileops_stress.c
      recovery_fileops_unit.c
      recovery_stress.c
      redirect.c
      replace-into-write-lock.c
      root_fifo_2.c
      root_fifo_32.c
      root_fifo_41.c
      shutdown-3344.c
      stat64.c
      stat64-create-modify-times.c
      stat64_flatten.c
      stat64-null-txn.c
      stat64-root-changes.c
      stress-gc.c
      stress-gc2.c
      test-xa-prepare.c
      test1324.c
      test1572.c
      test3219.c
      test3522.c
      test3522b.c
      test3529.c
      test938c.c
      test_3645.c
      test_3529_insert_2.c
      test_3529_table_lock.c
      test_3755.c
      test_4015.c
      test_4368.c
      test_4657.c
      test_5015.c
      test_abort1.c
      test_abort4.c
      test_abort5.c
      test_blobs_leaf_split.c
      test_bulk_fetch.c
      test_compression_methods.c
      test_cmp_descriptor.c
      test_db_change_xxx.c
      test_cursor_delete_2119.c
      test_db_descriptor.c
      test_db_descriptor_named_db.c
      test_db_txn_locks_read_uncommitted.c
      test_dupsort_get_both_range_reverse.c
      test_dupsort_set_range_reverse.c
      test_get_max_row_size.c
      test_large_update_broadcast_small_cachetable.c
      test_locktree_close.c
      test_logflush.c
      test_multiple_checkpoints_block_commit.c
      test_query.c
      test_rand_insert_mult_db.c
      test_redirect_func.c
      test_row_size_supported.c
      test_stress1.c
      test_stress2.c
      test_stress3.c
      test_stress4.c
      test_stress5.c
      test_stress6.c
      test_stress7.c
      test_stress_openclose.c
      test_stress_with_verify.c
      test_stress_hot_indexing.c
      test_transactional_descriptor.c
      test_trans_desc_during_chkpt.c
      test_trans_desc_during_chkpt2.c
      test_trans_desc_during_chkpt3.c
      test_trans_desc_during_chkpt4.c
      test_txn_abort6.c
      test_txn_abort8.c
      test_txn_abort9.c
      test_txn_close_open_commit.c
      test_txn_commit8.c
      test_txn_nested1.c
      test_txn_nested2.c
      test_txn_nested3.c
      test_txn_nested4.c
      test_txn_nested5.c
      test_update_abort_works.c
      test_update_calls_back.c
      test_update_can_delete_elements.c
      test_update_changes_values.c
      test_update_nonexistent_keys.c
      test_update_previously_deleted.c
      test_update_stress.c
      test_update_txn_snapshot_works_concurrently.c
      test_update_txn_snapshot_works_correctly_with_deletes.c
      test_update_broadcast_abort_works.c
      test_update_broadcast_calls_back.c
      test_update_broadcast_can_delete_elements.c
      test_update_broadcast_changes_values.c
      test_update_broadcast_previously_deleted.c
      test_update_broadcast_stress.c
      test_update_broadcast_update_fun_has_choices.c
      test_update_broadcast_with_empty_table.c
      test_update_broadcast_indexer.c
      test_update_broadcast_loader.c
      test_update_broadcast_nested_updates.c
      test_update_nested_updates.c
      test_update_with_empty_table.c
      txn-ignore.c
      transactional_fileops.c
      update-multiple-data-diagonal.c
      update-multiple-key0.c
      update-multiple-nochange.c
      update-multiple-with-indexer.c
      update.c
      upgrade_simple.c
      upgrade-test-1.c
      upgrade-test-2.c
      upgrade-test-3.c
      upgrade-test-4.c
      upgrade-test-5.c
      upgrade-test-6.c
      upgrade-test-7.c
      zombie_db.c
      )
    set(bdb_srcs ${srcs})
    list(REMOVE_ITEM bdb_srcs ${bdb_dontrun_srcs})
    string(REGEX REPLACE "\\.c(;|$)" ".bdb\\1" bdb_tests "${bdb_srcs}")
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
  add_executable(test-5138.tdb test-5138.c)
  target_link_libraries(test-5138.tdb ${LIBTOKUDB}_static ${LIBTOKUPORTABILITY}_static)
  set_property(TARGET test-5138.tdb APPEND PROPERTY
    COMPILE_DEFINITIONS "ENVDIR=\"dir.test-5138.tdb\";USE_TDB;IS_TDB=1;TOKUDB=1")
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.test-5138.tdb")
  add_common_options_to_binary_targets(test-5138.tdb)
  add_test(ydb/test-5138.tdb test-5138.tdb)

  foreach(bin ${tdb_bins})
    get_filename_component(base ${bin} NAME_WE)
    set(src ${base}.c)

    add_executable(${base}.tdb ${src})
    target_link_libraries(${base}.tdb ${LIBTOKUDB} ${LIBTOKUPORTABILITY})
    set_property(TARGET ${base}.tdb APPEND PROPERTY
      COMPILE_DEFINITIONS "ENVDIR=\"dir.${src}.tdb\";USE_TDB;IS_TDB=1;TOKUDB=1")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${src}.tdb")
    add_common_options_to_binary_targets(${base}.tdb)
  endforeach(bin)

  if(BDB_FOUND)
    foreach(bin ${bdb_bins})
      get_filename_component(base ${bin} NAME_WE)
      set(src ${base}.c)

      add_executable(${base}.bdb ${src})
      set_property(TARGET ${base}.bdb APPEND PROPERTY
        COMPILE_DEFINITIONS "ENVDIR=\"dir.${src}.bdb\";USE_BDB;IS_TDB=0;TOKU_ALLOW_DEPRECATED")
      set_target_properties(${base}.bdb PROPERTIES
        INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../portability;${CMAKE_CURRENT_SOURCE_DIR}/../..")
      target_link_libraries(${base}.bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${src}.bdb")
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
          $<TARGET_FILE:tokudb_dump> "${BDBDIR}/bin/db_dump")
    add_dependencies(test1426.tdb tokudb_dump)
  endif()

  string(REGEX REPLACE "\\.c(;|$)" ".tdb\\1" recover_would_be_tdb_tests "${recover_srcs}")
  declare_custom_tests(${recover_would_be_tdb_tests})

  declare_custom_tests(powerfail.tdb)
  add_test(ydb/powerfail.tdb echo must run powerfail by hand)

  declare_custom_tests(checkpoint_stress.tdb)
  configure_file(run_checkpoint_stress_test.sh . COPYONLY)
  add_test(NAME ydb/checkpoint_stress.tdb
    COMMAND run_checkpoint_stress_test.sh $<TARGET_FILE:checkpoint_stress.tdb> 5 5001 137)

  declare_custom_tests(recover_stress.tdb)
  add_custom_executable(recover stress.tdb checkpoint_stress.c)
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
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.recovery_fileops_unit.c.errors")

  if (NOT (CMAKE_SYSTEM_NAME MATCHES Darwin OR
           (CMAKE_C_COMPILER_ID STREQUAL Intel AND
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

  add_custom_executable(helgrind test_groupcommit_count.tdb test_groupcommit_count.c)
  add_helgrind_test(ydb/helgrind_test_groupcommit_count.tdb $<TARGET_FILE:helgrind_test_groupcommit_count.tdb> -n 1 -p hgrind)

  add_custom_executable(drd test_groupcommit_count.tdb test_groupcommit_count.c)
  add_drd_test(ydb/drd_test_groupcommit_count.tdb $<TARGET_FILE:drd_test_groupcommit_count.tdb> -n 1 -p drd)

  add_custom_executable(drd test_4015.tdb test_4015.c)
  add_drd_test(ydb/drd_test_4015.tdb $<TARGET_FILE:drd_test_4015.tdb>)

  configure_file(run_stress_test.sh . COPYONLY)
  foreach(src ${stress_test_srcs})
    if(NOT ${src} MATCHES hot_index)
      string(REGEX REPLACE "\\.c$" ".tdb" test "${src}")

      add_test(ydb/${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:${test}> 150000 1000)

      add_custom_executable(tiny ${test} ${src})
      add_drd_test(ydb/drd_tiny_${test} $<TARGET_FILE:tiny_${test}> --num_seconds 5 --num_elements 150)
      add_custom_executable(mid ${test} ${src})
      add_drd_test(ydb/drd_mid_${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:mid_${test}> 10000 100)
      add_custom_executable(large ${test} ${src})
      add_drd_test(ydb/drd_large_${test} ${CMAKE_CFG_INTDIR}/run_stress_test.sh $<TARGET_FILE:large_${test}> 150000 1000)
      set_tests_properties(ydb/drd_large_${test} PROPERTIES TIMEOUT 7200)
    endif()
  endforeach(src)

  configure_file(run_upgrade_stress_test.sh . COPYONLY)
  foreach(src ${stress_test_srcs})
    if (NOT ${src} MATCHES test_stress_openclose)
      string(REGEX REPLACE "\\.c$" ".tdb" test "${src}")

      foreach(oldver 4.2.0 5.0.8 5.2.7 6.0.0)
        foreach(p_or_s pristine stressed)
          if (NOT (${src} MATCHES test_stress4 AND ${p_or_s} MATCHES stressed))
            foreach(size 2000)
              add_custom_executable(upgrade_${oldver}_${p_or_s}_${size} ${test} ${src})
              add_test(
                NAME ydb/${test}/upgrade/${oldver}/${p_or_s}/${size}
                COMMAND ${CMAKE_CFG_INTDIR}/run_upgrade_stress_test.sh $<TARGET_FILE:upgrade_${oldver}_${p_or_s}_${size}_${test}> ${TOKU_SVNROOT}/tokudb.data/old-stress-test-envs/${oldver}/saved${p_or_s}-${size}-dir dir.upgrade_${oldver}_${p_or_s}_${size}_${src}.tdb ${size} 600)
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

  set_property(SOURCE loader-stress-test.c APPEND PROPERTY COMPILE_DEFINITIONS
    "OLDDATADIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../../../../tokudb.data/\"")
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
      COMMAND run_recover_test.sh $<TARGET_FILE:${base}.tdb> "dir.${base}.c.tdb" $<TARGET_FILE:tdb-recover> $<TARGET_FILE:tokudb_dump_static>)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "dir.${base}.c.tdb" "dir.${base}.c.tdb.recover")
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
    ydb/env-put-multiple.tdb
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
    ydb/test_abort4_19_0.tdb
    ydb/test_abort4_19_1.tdb
    ydb/test_abort5.tdb
    ydb/test_archive1.tdb
    ydb/test_logmax.tdb
    ydb/test_query.tdb
    ydb/test_rand_insert_mult_db.tdb
    ydb/test_txn_abort5.tdb
    ydb/test_txn_abort5a.tdb
    ydb/test_txn_abort6.tdb
    ydb/test_txn_nested2.tdb
    ydb/test_txn_nested4.tdb
    ydb/test_txn_nested5.tdb
    ydb/test_update_stress.tdb
    )
  if (BDB_FOUND)
    list(APPEND long_tests
      ydb/root_fifo_1.bdb
      ydb/root_fifo_31.bdb
      ydb/rowsize.bdb
      ydb/test1426.tdb
      ydb/test_groupcommit_count.bdb
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
    ydb/recovery_fileops_unit.tdb
    ydb/test3039.tdb
    )
  set_tests_properties(${extra_long_tests} PROPERTIES TIMEOUT 7200)
  ## these really take a long time with valgrind
  set(phenomenally_long_tests
    ydb/checkpoint_stress.tdb
    ydb/recover_stress.tdb
    )
  set_tests_properties(${phenomenally_long_tests} PROPERTIES TIMEOUT 14400)
endif(BUILD_TESTING)
