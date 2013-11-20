set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

if(BUILD_TESTING OR BUILD_SRC_TESTS)
  function(add_ydb_test bin)
    add_toku_test(ydb ${bin} ${ARGN})
  endfunction(add_ydb_test)
  function(add_ydb_test_aux name bin)
    add_toku_test_aux(ydb ${name} ${bin} ${ARGN})
  endfunction(add_ydb_test_aux)

  function(add_ydb_helgrind_test bin)
    add_helgrind_test(ydb helgrind_${bin} $<TARGET_FILE:${bin}> ${ARGN})
  endfunction(add_ydb_helgrind_test)
  function(add_ydb_drd_test_aux name bin)
    add_drd_test(ydb ${name} $<TARGET_FILE:${bin}> ${ARGN})
  endfunction(add_ydb_drd_test_aux)
  function(add_ydb_drd_test bin)
    add_ydb_drd_test_aux(drd_${bin} ${bin} ${ARGN})
  endfunction(add_ydb_drd_test)

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
      bigtxn27
      blackhole
      blocking-prelock-range
      blocking-set-range-reverse-0
      blocking-table-lock
      bug1381
      bug627
      cachetable-race
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
      dump-env
      env_loader_memory
      env-put-multiple
      env_startup
      execute-updates
      filesize
      get_key_after_bytes_unit
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
      mvcc-read-committed
      openlimit17
      openlimit17-locktree
      openlimit17-metafiles
      perf_checkpoint_var
      perf_child_txn
      perf_cursor_nop
      perf_iibench
      perf_insert
      perf_insert_multiple
      perf_malloc_free
      perf_nop
      perf_ptquery
      perf_ptquery2
      perf_rangequery
      perf_read_txn
      perf_read_txn_single_thread
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
      put-del-multiple-array-indexing
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
      seqinsert
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
      test_cursor_with_read_txn
      test_db_change_pagesize
      test_db_change_xxx
      test_cursor_delete_2119
      test_db_descriptor
      test_db_descriptor_named_db
      test_db_txn_locks_read_uncommitted
      test_get_max_row_size
      test_iterate_live_transactions
      test_iterate_pending_lock_requests
      test_large_update_broadcast_small_cachetable
      test_lock_timeout_callback
      test_locking_with_read_txn
      test_locktree_close
      test_logflush
      test_multiple_checkpoints_block_commit
      test_query
      test_read_txn_invalid_ops
      test_redirect_func
      test_restrict
      test_row_size_supported
      test_simple_read_txn
      test_stress0
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
      test_updates_single_key
      txn-ignore
      transactional_fileops
      update-multiple-data-diagonal
      update-multiple-key0
      update-multiple-nochange
      update-multiple-with-indexer
      update-multiple-with-indexer-array
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
    loader-tpch-load.loader
    )

  ## #5138 only reproduces when using the static library.
  list(REMOVE_ITEM tdb_bins test-5138.tdb)
  add_executable(test-5138.tdb test-5138.cc)
  target_link_libraries(test-5138.tdb ${LIBTOKUDB}_static ${ZLIB_LIBRARY} ${LIBTOKUPORTABILITY}_static ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_SYSTEM_LIBS})
  set_property(TARGET test-5138.tdb APPEND PROPERTY
    COMPILE_DEFINITIONS "USE_TDB;IS_TDB=1;TOKUDB=1")
  add_space_separated_property(TARGET test-5138.tdb COMPILE_FLAGS -fvisibility=hidden)
  add_ydb_test(test-5138.tdb)

  foreach(bin ${tdb_bins})
    get_filename_component(base ${bin} NAME_WE)

    add_executable(${base}.tdb ${base}.cc)
    # Some of the symbols in util may not be exported properly by
    # libtokudb.so.
    # We link the test with util directly so that the test code itself can use
    # some of those things (i.e. kibbutz in the threaded tests).
    target_link_libraries(${base}.tdb util ${LIBTOKUDB} ${LIBTOKUPORTABILITY})
    set_property(TARGET ${base}.tdb APPEND PROPERTY
      COMPILE_DEFINITIONS "USE_TDB;IS_TDB=1;TOKUDB=1")
    add_space_separated_property(TARGET ${base}.tdb COMPILE_FLAGS -fvisibility=hidden)
  endforeach(bin)

  if(BDB_FOUND)
    foreach(bin ${bdb_bins})
      get_filename_component(base ${bin} NAME_WE)

      add_executable(${base}.bdb ${base}.cc)
      set_property(TARGET ${base}.bdb APPEND PROPERTY
        COMPILE_DEFINITIONS "USE_BDB;IS_TDB=0;TOKU_ALLOW_DEPRECATED")
      set_target_properties(${base}.bdb PROPERTIES
        INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../../portability;${CMAKE_CURRENT_SOURCE_DIR}/../..")
      target_link_libraries(${base}.bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
      add_space_separated_property(TARGET ${base}.bdb COMPILE_FLAGS -fvisibility=hidden)
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
          "test1426.tdb.ctest-data" "test1426.bdb.ctest-data"
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
  setup_toku_test_properties(ydb/checkpoint_stress.tdb checkpoint_stress.tdb)

  configure_file(run_recover_stress_test.sh . COPYONLY)
  add_test(NAME ydb/recover_stress.tdb
    COMMAND run_recover_stress_test.sh $<TARGET_FILE:checkpoint_stress.tdb> 5 5001 137)
  setup_toku_test_properties(ydb/recover_stress.tdb recover_stress.tdb)

  declare_custom_tests(diskfull.tdb)
  configure_file(run_diskfull_test.sh . COPYONLY)
  add_test(NAME ydb/diskfull.tdb
    COMMAND run_diskfull_test.sh $<TARGET_FILE:diskfull.tdb> 134)
  setup_toku_test_properties(ydb/diskfull.tdb diskfull.tdb)

  declare_custom_tests(recovery_fileops_unit.tdb)
  configure_file(run_recovery_fileops_unit.sh . COPYONLY)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/recovery_fileops_unit_dir")
  foreach(ov c d r)

    if (ov STREQUAL c)
      set(gset 0)
      set(hset 0)
    else ()
      set(gset 0 1 2 3 4 5)
      set(hset 0 1)
    endif ()

    foreach(av 0 1)
      foreach(bv 0 1)

        if (bv)
          set(dset 0 1)
          set(eset 0 1)
        else ()
          set(dset 0)
          set(eset 0)
        endif ()

        foreach(cv 0 1 2)
          foreach(dv ${dset})
            foreach(ev ${eset})
              foreach(fv 0 1)
                foreach(gv ${gset})
                  foreach(hv ${hset})

                    if ((NOT ov STREQUAL c) AND (NOT cv) AND ((NOT bv) OR (NOT ev) OR (dv)))
                      set(iset 0 1)
                    else ()
                      set(iset 0)
                    endif ()

                    foreach(iv ${iset})
                      set(testname "ydb/recovery_fileops_unit.${ov}${av}${bv}${cv}${dv}${ev}${fv}${gv}${hv}${iv}")
                      set(envdir "recovery_fileops_unit_dir/${ov}${av}${bv}${cv}${dv}${ev}${fv}${gv}${hv}${iv}")
                      set(errfile "recovery_fileops_unit_dir/${ov}${av}${bv}${cv}${dv}${ev}${fv}${gv}${hv}${iv}.ctest-errors")
                      add_test(NAME ${testname}
                        COMMAND run_recovery_fileops_unit.sh $<TARGET_FILE:recovery_fileops_unit.tdb> ${errfile} 137
                        -O ${ov} -A ${av} -B ${bv} -C ${cv} -D ${dv} -E ${ev} -F ${fv} -G ${gv} -H ${hv} -I ${iv}
                        )
                      setup_toku_test_properties(${testname} ${envdir})
                      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${errfile}")
                    endforeach(iv)
                  endforeach(hv)
                endforeach(gv)
              endforeach(fv)
            endforeach(ev)
          endforeach(dv)
        endforeach(cv)
      endforeach(bv)
    endforeach(av)
  endforeach(ov)

  if (NOT (CMAKE_SYSTEM_NAME MATCHES Darwin OR
           (CMAKE_CXX_COMPILER_ID STREQUAL Intel AND
            CMAKE_BUILD_TYPE STREQUAL Release)
           OR USE_GCOV))
    declare_custom_tests(helgrind1.tdb)
    add_test(NAME ydb/helgrind_helgrind1.tdb
      COMMAND valgrind --quiet --tool=helgrind --error-exitcode=1 --log-file=helgrind1.tdb.deleteme $<TARGET_FILE:helgrind1.tdb>)
    setup_toku_test_properties(ydb/helgrind_helgrind1.tdb helgrind_helgrind1.tdb)
    set_tests_properties(ydb/helgrind_helgrind1.tdb PROPERTIES WILL_FAIL TRUE)
  endif()
  declare_custom_tests(helgrind2.tdb)
  declare_custom_tests(helgrind3.tdb)
  add_ydb_helgrind_test(helgrind2.tdb)
  add_ydb_helgrind_test(helgrind3.tdb)

  declare_custom_tests(test_groupcommit_count.tdb)
  add_ydb_test(test_groupcommit_count.tdb -n 1)
  add_ydb_helgrind_test(test_groupcommit_count.tdb -n 1)
  add_ydb_drd_test(test_groupcommit_count.tdb -n 1)

  add_ydb_drd_test(test_4015.tdb)

  # We link the locktree so that stress test 0 can call some
  # functions (ie: lock escalation) directly.
  target_link_libraries(test_stress0.tdb locktree)

  # Set up default stress tests and drd tests. Exclude hot_index.
  foreach(src ${stress_test_srcs})
    if(NOT ${src} MATCHES hot_index)
      get_filename_component(base ${src} NAME_WE)
      set(test ${base}.tdb)

      if (${src} MATCHES test_stress0)
        add_ydb_test(${test} --num_elements 512 --num_seconds 1000 --join_timeout 600)
      else ()
        add_ydb_test(${test} --num_elements 150000 --num_seconds 1000 --join_timeout 600)
      endif ()

      add_ydb_drd_test_aux(drd_tiny_${test} ${test} --num_seconds 5 --num_elements 150 --join_timeout 3000)
      set_tests_properties(ydb/drd_tiny_${test} PROPERTIES TIMEOUT 3600)

      add_test(ydb/drd_mid_${test}/prepare ${test} --only_create --num_elements 10000)
      setup_toku_test_properties(ydb/drd_mid_${test}/prepare drd_mid_${test})
      add_ydb_drd_test_aux(drd_mid_${test} ${test} --only_stress --num_elements 10000 --num_seconds 100 --join_timeout 14400)
      set_tests_properties(ydb/drd_mid_${test} PROPERTIES
        DEPENDS ydb/drd_mid_${test}/prepare
        REQUIRED_FILES "drd_mid_${test}.ctest-data"
        TIMEOUT 15000
        )

      add_test(ydb/drd_large_${test}/prepare ${test} --only_create --num_elements 150000)
      setup_toku_test_properties(ydb/drd_large_${test}/prepare drd_large_${test})
      add_ydb_drd_test_aux(drd_large_${test} ${test} --only_stress --num_elements 150000 --num_seconds 1000 --join_timeout 28800)
      set_tests_properties(ydb/drd_large_${test} PROPERTIES
        DEPENDS ydb/drd_large_${test}/prepare
        REQUIRED_FILES "drd_large_${test}.ctest-data"
        TIMEOUT 30000
        )
    endif()
  endforeach(src)

  # Set up upgrade tests. Exclude test_stress_openclose
  foreach(src ${stress_test_srcs})
    if (NOT ${src} MATCHES test_stress_openclose)
      get_filename_component(base ${src} NAME_WE)
      set(test ${base}.tdb)

      foreach(oldver 4.2.0 5.0.8 5.2.7 6.0.0 6.1.0 6.5.1 6.6.3)
        set(versiondir ${TOKUDB_DATA}/old-stress-test-envs/${oldver})
        if (NOT EXISTS "${versiondir}/" AND NOT WARNED_ABOUT_DATA)
          message(WARNING "Test data for upgrade tests for version ${oldver} doesn't exist, check out ${versiondir}/*-2000-dir first or upgrade stress tests may fail.")
          set(WARNED_ABOUT_DATA 1)
        endif ()
        foreach(p_or_s pristine stressed)
          if (NOT (${base} MATCHES test_stress4 AND ${p_or_s} MATCHES stressed))
            foreach(size 2000)
              set(oldenvdir "${versiondir}/saved${p_or_s}-${size}-dir")
              set(envdirbase "${upgrade}_${oldver}_${p_or_s}_${size}_${test}")
              set(envdir "${envdirbase}.ctest-data")
              set(testnamebase ydb/${test}/upgrade/${oldver}/${p_or_s}/${size})

              add_test(NAME ${testnamebase}/remove
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${envdir}")
              add_test(NAME ${testnamebase}/copy
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${oldenvdir}" "${envdir}")
              set_tests_properties(${testnamebase}/copy PROPERTIES
                DEPENDS ${testnamebase}/remove
                REQUIRED_FILES "${oldenvdir}")

              add_test(NAME ${testnamebase}
                COMMAND ${test} --only_stress --num_elements ${size} --num_seconds 600 --join_timeout 7200)
              setup_toku_test_properties(${testnamebase} "${envdirbase}")
              set_tests_properties(${testnamebase} PROPERTIES
                DEPENDS ${testnamebase}/copy
                REQUIRED_FILES "${envdir}"
                TIMEOUT 10800)
            endforeach(size)
          endif ()
        endforeach(p_or_s)
      endforeach(oldver)
    endif ()
  endforeach(src)

  if (NOT EXISTS "${TOKUDB_DATA}/test_5902/" AND NOT WARNED_ABOUT_DATA)
    message(WARNING "Test data for dump-env.tdb doesn't exist, check out ${TOKUDB_DATA}/test_5902 first or dump-env.tdb may fail.")
    set(WARNED_ABOUT_DATA 1)
  endif ()
  declare_custom_tests(dump-env.tdb)
  add_test(NAME ydb/dump-env.tdb/remove
    COMMAND ${CMAKE_COMMAND} -E remove_directory "dump-env.tdb.ctest-data")
  add_test(NAME ydb/dump-env.tdb/copy
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TOKUDB_DATA}/test_5902" "dump-env.tdb.ctest-data")
  set_tests_properties(ydb/dump-env.tdb/copy PROPERTIES
    DEPENDS ydb/dump-env.tdb/remove
    REQUIRED_FILES "${TOKUDB_DATA}/test_5902")
  add_ydb_test(dump-env.tdb)
  set_tests_properties(ydb/dump-env.tdb PROPERTIES
    DEPENDS ydb/dump-env.tdb/copy
    REQUIRED_FILES "dump-env.tdb.ctest-data")

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
    add_ydb_test_aux(root_fifo_41_${num}_populate.tdb root_fifo_41.tdb -n ${num} -populate)
    add_ydb_test_aux(root_fifo_41_${num}_nopopulate.tdb root_fifo_41.tdb -n ${num})
  endforeach(num)

  add_ydb_test_aux(test3039_small.tdb test3039.tdb -n 1000)

  declare_custom_tests(test_abort4.tdb)
  foreach(num RANGE -1 19)
    add_ydb_test_aux(test_abort4_${num}_0.tdb test_abort4.tdb -c 0 -l ${num})
    add_ydb_test_aux(test_abort4_${num}_1.tdb test_abort4.tdb -c 1 -l ${num})
  endforeach(num)

  set(old_loader_upgrade_data "${TOKUDB_DATA}/env_preload.4.2.0.emptydictionaries.cleanshutdown")
  if (NOT EXISTS "${old_loader_upgrade_data}/" AND NOT WARNED_ABOUT_DATA)
    message(WARNING "Test data for loader upgrade tests doesn't exist, check out ${old_loader_upgrade_data} first, or loader-stress-test3.tdb may fail.")
    set(WARNED_ABOUT_DATA 1)
  endif ()
  function(add_loader_upgrade_test name bin)
    add_test(NAME ydb/${name}/remove
      COMMAND ${CMAKE_COMMAND} -E remove_directory "${name}.ctest-data")
    add_test(NAME ydb/${name}/copy
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${old_loader_upgrade_data}" "${name}.ctest-data")
    set_tests_properties(ydb/${name}/copy PROPERTIES
      DEPENDS ydb/${name}/remove
      REQUIRED_FILES "${old_loader_upgrade_data}")
    add_ydb_test_aux(${name} ${bin} -u ${ARGN})
    set_tests_properties(ydb/${name} PROPERTIES
      DEPENDS ydb/${name}/copy
      REQUIRED_FILES "${name}.ctest-data")
  endfunction(add_loader_upgrade_test)

  list(REMOVE_ITEM loader_tests loader-stress-test.loader)
  add_ydb_test_aux(loader-stress-test0.tdb loader-stress-test.tdb -c)
  add_ydb_test_aux(loader-stress-test1.tdb loader-stress-test.tdb -c -p)
  add_ydb_test_aux(loader-stress-test2.tdb loader-stress-test.tdb -r 5000 -s)
  add_loader_upgrade_test(loader-stress-test3.tdb loader-stress-test.tdb -c)
  add_ydb_test_aux(loader-stress-test4.tdb loader-stress-test.tdb -r 10000000 -c)
  add_ydb_test_aux(loader-stress-test0z.tdb loader-stress-test.tdb -c -z)
  add_ydb_test_aux(loader-stress-test1z.tdb loader-stress-test.tdb -c -p -z)
  add_ydb_test_aux(loader-stress-test2z.tdb loader-stress-test.tdb -r 5000 -s -z)
  add_loader_upgrade_test(loader-stress-test3z.tdb loader-stress-test.tdb -c -z)
  add_ydb_test_aux(loader-stress-test4z.tdb loader-stress-test.tdb -r 500000 -c -z --valsize 28)

  list(REMOVE_ITEM loader_tests loader-dup-test.loader)
  add_ydb_test_aux(loader-dup-test0.tdb loader-dup-test.tdb)
  add_ydb_test_aux(loader-dup-test1.tdb loader-dup-test.tdb -d 1 -r 500000)
  add_ydb_test_aux(loader-dup-test2.tdb loader-dup-test.tdb -d 1 -r 1000000)
  add_ydb_test_aux(loader-dup-test3.tdb loader-dup-test.tdb -d 1 -s -r 100)
  add_ydb_test_aux(loader-dup-test4.tdb loader-dup-test.tdb -d 1 -s -r 1000)
  add_ydb_test_aux(loader-dup-test5.tdb loader-dup-test.tdb -d 1 -s -r 1000 -E)
  add_ydb_test_aux(loader-dup-test0z.tdb loader-dup-test.tdb -z)
  add_ydb_test_aux(loader-dup-test1z.tdb loader-dup-test.tdb -d 1 -r 500000 -z)
  add_ydb_test_aux(loader-dup-test2z.tdb loader-dup-test.tdb -d 1 -r 1000000 -z)
  add_ydb_test_aux(loader-dup-test3z.tdb loader-dup-test.tdb -d 1 -s -r 100 -z)
  add_ydb_test_aux(loader-dup-test4z.tdb loader-dup-test.tdb -d 1 -s -r 1000 -z)
  add_ydb_test_aux(loader-dup-test5z.tdb loader-dup-test.tdb -d 1 -s -r 1000 -E -z)

  ## as part of #4503, we took out test 1 and 3
  list(REMOVE_ITEM loader_tests loader-cleanup-test.loader)
  add_ydb_test_aux(loader-cleanup-test0.tdb loader-cleanup-test.tdb -s -r 800)
  #add_ydb_test_aux(loader-cleanup-test1.tdb loader-cleanup-test.tdb -s -r 800 -p)
  add_ydb_test_aux(loader-cleanup-test2.tdb loader-cleanup-test.tdb -s -r 8000)
  #add_ydb_test_aux(loader-cleanup-test3.tdb loader-cleanup-test.tdb -s -r 8000 -p)
  add_ydb_test_aux(loader-cleanup-test0z.tdb loader-cleanup-test.tdb -s -r 800 -z)
  add_ydb_test_aux(loader-cleanup-test2z.tdb loader-cleanup-test.tdb -s -r 8000 -z)

  declare_custom_tests(keyrange.tdb)
  add_ydb_test_aux(keyrange-get0.tdb keyrange.tdb --get 0)
  add_ydb_test_aux(keyrange-get1.tdb keyrange.tdb --get 1)
  add_ydb_test_aux(keyrange-random-get0.tdb keyrange.tdb --get 0 --random_keys 1)
  add_ydb_test_aux(keyrange-random-get1.tdb keyrange.tdb --get 1 --random_keys 1)
  add_ydb_test_aux(keyrange-loader-get0.tdb keyrange.tdb --get 0 --loader 1)
  add_ydb_test_aux(keyrange-loader-get1.tdb keyrange.tdb --get 1 --loader 1)

  declare_custom_tests(maxsize-for-loader.tdb)
  add_ydb_test_aux(maxsize-for-loader-A.tdb maxsize-for-loader.tdb -f -c)
  add_ydb_test_aux(maxsize-for-loader-B.tdb maxsize-for-loader.tdb -c)
  add_ydb_test_aux(maxsize-for-loader-Az.tdb maxsize-for-loader.tdb -f -z -c)
  add_ydb_test_aux(maxsize-for-loader-Bz.tdb maxsize-for-loader.tdb -z -c)

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
    setup_toku_test_properties(ydb/${test} ${test})
  endforeach()

  foreach(test ${tdb_tests} ${bdb_tests})
    add_ydb_test(${test})
  endforeach(test)

  configure_file(run_recover_test.sh . COPYONLY)
  foreach(recover_test ${recover_tests})
    get_filename_component(base ${recover_test} NAME_WE)
    add_test(NAME ydb/${recover_test}
      COMMAND run_recover_test.sh $<TARGET_FILE:${base}.tdb> "${recover_test}.ctest-data" $<TARGET_FILE:tdb-recover> $<TARGET_FILE:tokudb_dump>)
    setup_toku_test_properties(ydb/${recover_test} ${recover_test})
  endforeach(recover_test)

  configure_file(run_abortrecover_test.sh . COPYONLY)
  foreach(abortrecover_test ${abortrecover_tests})
    get_filename_component(base ${abortrecover_test} NAME_WE)
    add_test(NAME ydb/${abortrecover_test}
      COMMAND run_abortrecover_test.sh $<TARGET_FILE:${base}.tdb>)
    setup_toku_test_properties(ydb/${abortrecover_test} ${abortrecover_test})
  endforeach(abortrecover_test)
  ## alternate implementation, doesn't work because the abort phase crashes and we can't tell cmake that's expected
  # foreach(abortrecover_test ${abortrecover_tests})
  #   get_filename_component(base ${abortrecover_test} NAME_WE)
  #   set(test ${base}.tdb)
  #   add_test(NAME ydb/${test}/abort
  #     COMMAND ${test} --test)
  #   setup_toku_test_properties(ydb/${test}/abort ${abortrecover_test})
  #   set_tests_properties(ydb/${test}/abort PROPERTIES WILL_FAIL TRUE)

  #   add_test(NAME ydb/${test}/recover
  #     COMMAND ${test} --recover)
  #   setup_toku_test_properties(ydb/${test}/recover ${abortrecover_test})
  #   set_tests_properties(ydb/${test}/recover PROPERTIES
  #     DEPENDS ydb/${test}/abort
  #     REQUIRED_FILES "${abortrecover_test}.ctest-data")
  # endforeach(abortrecover_test)

  foreach(loader_test ${loader_tests})
    get_filename_component(base ${loader_test} NAME_WE)
    add_ydb_test_aux(${base}.nop.loader ${base}.tdb)
    add_ydb_test_aux(${base}.p.loader ${base}.tdb -p)
    add_ydb_test_aux(${base}.comp.loader ${base}.tdb -z)
    if("${tdb_tests_that_should_fail}" MATCHES "${base}.loader")
      list(REMOVE_ITEM tdb_tests_that_should_fail ${base}.loader)
      list(APPEND tdb_tests_that_should_fail ${base}.nop.loader ${base}.p.loader ${base}.comp.loader)
    endif()
  endforeach(loader_test)

  set(tdb_tests_that_should_fail "ydb/${tdb_tests_that_should_fail}")
  string(REGEX REPLACE ";" ";ydb/" tdb_tests_that_should_fail "${tdb_tests_that_should_fail}")
  set_tests_properties(${tdb_tests_that_should_fail} PROPERTIES WILL_FAIL TRUE)

  ## give some tests, that time out normally, 1 hour to complete
  set(long_tests
    ydb/drd_test_groupcommit_count.tdb
    ydb/env-put-multiple.tdb
    ydb/filesize.tdb
    ydb/loader-cleanup-test0.tdb
    ydb/loader-cleanup-test0z.tdb
    ydb/manyfiles.tdb
    ydb/recover-loader-test.abortrecover
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
    ydb/test_txn_abort5.tdb
    ydb/test_txn_abort5a.tdb
    ydb/test_txn_abort6.tdb
    ydb/test_txn_nested2.tdb
    ydb/test_txn_nested4.tdb
    ydb/test_txn_nested5.tdb
    ydb/test_update_broadcast_stress.tdb
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
    ydb/hot-optimize-table-tests.tdb
    ydb/loader-cleanup-test2.tdb
    ydb/loader-cleanup-test2z.tdb
    ydb/loader-dup-test0.tdb
    ydb/loader-stress-del.nop.loader
    ydb/loader-stress-del.p.loader
    ydb/loader-stress-del.comp.loader
    ydb/test3039.tdb
    ydb/test_update_stress.tdb
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
    ydb/loader-stress-test4.tdb
    ydb/loader-stress-test4z.tdb
    ydb/recover_stress.tdb
    ydb/test3529.tdb
    )
  if (BDB_FOUND)
    list(APPEND phenomenally_long_tests
      ydb/test1426.tdb
      )
  endif (BDB_FOUND)
  set_tests_properties(${phenomenally_long_tests} PROPERTIES TIMEOUT 14400)
endif(BUILD_TESTING OR BUILD_SRC_TESTS)
