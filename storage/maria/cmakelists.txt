SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
ADD_LIBRARY(myisam ft_boolean_search.c ft_nlq_search.c ft_parser.c ft_static.c ft_stem.c
				ft_stopwords.c ft_update.c mi_cache.c mi_changed.c mi_check.c
				mi_checksum.c mi_close.c mi_create.c mi_dbug.c mi_delete.c 
				mi_delete_all.c mi_delete_table.c mi_dynrec.c mi_extra.c mi_info.c
				mi_key.c mi_keycache.c mi_locking.c mi_log.c mi_open.c 
				mi_packrec.c mi_page.c mi_panic.c mi_preload.c mi_range.c mi_rename.c
				mi_rfirst.c mi_rlast.c mi_rnext.c mi_rnext_same.c mi_rprev.c mi_rrnd.c
				mi_rsame.c mi_rsamepos.c mi_scan.c mi_search.c mi_static.c mi_statrec.c
				mi_unique.c mi_update.c mi_write.c rt_index.c rt_key.c rt_mbr.c
				rt_split.c sort.c sp_key.c ft_eval.h myisamdef.h rt_index.h mi_rkey.c)

ADD_EXECUTABLE(myisam_ftdump myisam_ftdump.c)
TARGET_LINK_LIBRARIES(myisam_ftdump myisam mysys dbug strings zlib wsock32)

ADD_EXECUTABLE(myisamchk myisamchk.c)
TARGET_LINK_LIBRARIES(myisamchk myisam mysys dbug strings zlib wsock32)

ADD_EXECUTABLE(myisamlog myisamlog.c)
TARGET_LINK_LIBRARIES(myisamlog myisam mysys dbug strings zlib wsock32)

ADD_EXECUTABLE(myisampack myisampack.c)
TARGET_LINK_LIBRARIES(myisampack myisam mysys dbug strings zlib wsock32)
