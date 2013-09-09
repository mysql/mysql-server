include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(locktree_srcs
    locktree
    manager
    lock_request
    txnid_set
    range_buffer
    keyrange
    wfg
    )

## make the shared library for tests
add_library(locktree SHARED ${locktree_srcs})
add_dependencies(locktree install_tdb_h)

## make the real library, it's going to go into libtokudb.so so it needs
## to be PIC
add_library(locktree_static STATIC ${locktree_srcs})
add_space_separated_property(TARGET locktree_static COMPILE_FLAGS "-fvisibility=hidden -fPIC")
add_dependencies(locktree_static install_tdb_h)
# The locktree uses the standard portability layer and also the ybt 
# functions from the ft layer. Maybe one day the ybt functions will be
# elsewhere.
target_link_libraries(locktree LINK_PRIVATE ft ${LIBTOKUPORTABILITY})

maybe_add_gcov_to_libraries(locktree locktree_static)

add_subdirectory(tests)
