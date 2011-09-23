# Source files for this module 
set(UNIT_SOURCE_FILES
    harness.cc
    alloc.cc
    cas.cc
    casbits.cc
    connpool.cc
    incr.cc
    queue.cc
    stub_logger.c
    tsv.cc
 )


add_executable(run_unit_tests ${UNIT_SOURCE_FILES})
target_link_libraries(run_unit_tests ndbmemcache)

add_executable(test_workqueue test_workqueue.c ../src/workqueue.c)

# Tests which can run without a cluster:

enable_testing()
add_test(cas_bitshifting run_unit_tests -t 2)
add_test(allocator       run_unit_tests -t 4)
add_test(tsv             run_unit_tests -t 6)
add_test(queue           run_unit_tests -t 7)
