SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
ADD_LIBRARY(heap _check.c _rectest.c hp_block.c hp_clear.c hp_close.c hp_create.c
				hp_delete.c hp_extra.c hp_hash.c hp_info.c hp_open.c hp_panic.c
				hp_rename.c hp_rfirst.c hp_rkey.c hp_rlast.c hp_rnext.c hp_rprev.c
				hp_rrnd.c hp_rsame.c hp_scan.c hp_static.c hp_update.c hp_write.c)
