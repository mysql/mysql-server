set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-variable")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=class-memaccess")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=sign-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-qual")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-misleading-indentation")


# find_package(Boost 1.66.0 COMPONENTS coroutine REQUIRED)

add_subdirectory(thirdparty/rlib)

set(STATE_SOURCE state_node/state_server.cc thirdparty/cjson/cJSON.cc)
add_executable(state_node ${STATE_SOURCE})
target_link_libraries(state_node rlib)

set(RDMA_SRC
        rdma_connection/meta_manager.cc
        rdma_connection/qp_manager.cc
        allocator/region_allocator.cc
        coroutine_scheduler/coroutine_scheduler.cc
        thirdparty/cjson/cJSON.cc
        #        util/redo_log_util.h
)

add_library(state_core STATIC ${RDMA_SRC})

target_link_libraries(state_core rlib pthread)