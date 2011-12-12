

######### run_unit_tests ##############
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
    lookuptable.cc
 )
add_executable(run_unit_tests ${UNIT_SOURCE_FILES})
target_link_libraries(run_unit_tests ndbmemcache)
##########################################


### test_workqueue #####################
set(TEST_WORKQUEUE_SOURCES test_workqueue.c ../src/workqueue.c) 
if(HAVE_SOLARIS_ATOMICS) 
  set(TEST_WORKQUEUE_SOURCES ${TEST_WORKQUEUE_SOURCES} ../src/atomics.c)
endif()

add_executable(test_workqueue ${TEST_WORKQUEUE_SOURCES})

if(LIBRT) 
  target_link_libraries(test_workqueue rt)
endif()
##########################################
