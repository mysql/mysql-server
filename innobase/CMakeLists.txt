#SET(CMAKE_CXX_FLAGS_DEBUG "-DSAFEMALLOC -DSAFE_MUTEX")
#SET(CMAKE_C_FLAGS_DEBUG "-DSAFEMALLOC -DSAFE_MUTEX")
ADD_DEFINITIONS(-DMYSQL_SERVER -D_WIN32 -DWIN32 -D_LIB)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include include)
ADD_LIBRARY(innobase btr/btr0btr.c btr/btr0cur.c btr/btr0pcur.c btr/btr0sea.c 
					 buf/buf0buf.c buf/buf0flu.c buf/buf0lru.c buf/buf0rea.c 
					 data/data0data.c data/data0type.c 
					 dict/dict0boot.c dict/dict0crea.c dict/dict0dict.c dict/dict0load.c dict/dict0mem.c 
					 dyn/dyn0dyn.c 
					 eval/eval0eval.c eval/eval0proc.c 
					 fil/fil0fil.c 
					 fsp/fsp0fsp.c
					 fut/fut0fut.c fut/fut0lst.c 
					 ha/ha0ha.c ha/hash0hash.c 
					 ibuf/ibuf0ibuf.c 
					 pars/lexyy.c pars/pars0grm.c pars/pars0opt.c pars/pars0pars.c pars/pars0sym.c 
					 lock/lock0lock.c
					 log/log0log.c log/log0recv.c 
					 mach/mach0data.c 
					 mem/mem0mem.c mem/mem0pool.c 
					 mtr/mtr0log.c mtr/mtr0mtr.c 
					 os/os0file.c os/os0proc.c os/os0sync.c os/os0thread.c 
					 page/page0cur.c page/page0page.c 
					 que/que0que.c 
					 read/read0read.c 
					 rem/rem0cmp.c rem/rem0rec.c
					 row/row0ins.c row/row0mysql.c row/row0purge.c row/row0row.c row/row0sel.c row/row0uins.c 
					 row/row0umod.c row/row0undo.c row/row0upd.c row/row0vers.c 
					 srv/srv0que.c srv/srv0srv.c srv/srv0start.c 
					 sync/sync0arr.c sync/sync0rw.c sync/sync0sync.c 
					 thr/thr0loc.c 
					 trx/trx0purge.c trx/trx0rec.c trx/trx0roll.c trx/trx0rseg.c trx/trx0sys.c trx/trx0trx.c trx/trx0undo.c 
					 usr/usr0sess.c 
					 ut/ut0byte.c ut/ut0dbg.c ut/ut0mem.c ut/ut0rnd.c ut/ut0ut.c )
