SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
ADD_LIBRARY(myisammrg myrg_close.c myrg_create.c myrg_delete.c myrg_extra.c myrg_info.c
				myrg_locking.c myrg_open.c myrg_panic.c myrg_queue.c myrg_range.c
				myrg_rfirst.c myrg_rkey.c myrg_rlast.c myrg_rnext.c myrg_rnext_same.c
				myrg_rprev.c myrg_rrnd.c myrg_rsame.c myrg_static.c myrg_update.c
				myrg_write.c)
