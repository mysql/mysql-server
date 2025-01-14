IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	SET(SPARROW_LIBS aio)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	SET(SPARROW_LIBS dnsapi)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	SET(SPARROW_LIBS nsl resolv)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	SET(SPARROW_LIBS resolv)
ENDIF()

SET(SPARROW_SOURCES
	handler/field.h
	handler/hasparrow.h
	handler/plugin.h
	handler/hasparrow.cc
	handler/field.cc
	handler/plugin.cc
	engine/alter.h
	engine/atomic.h
	engine/binbuffer.h
	engine/cache.h
	engine/coalescing.h
	engine/compress.h
	engine/cond.h
	engine/condition.h
	engine/context.h
	engine/exception.h
	engine/fileutil.h
	engine/flush.h
	engine/hash.h
	engine/internalapi.h
	engine/interval.h
	engine/intervaltree.h
	engine/io.h
	engine/list.h
	engine/listener.h
	engine/lock.h
	engine/log.h
	engine/master.h
	engine/misc.h
	engine/partition.h
	engine/persistent.h
	engine/purge.h
	engine/queue.h
	engine/scheduler.h
	engine/search.h
	engine/sema.h
	engine/serial.h
	engine/socketutil.h
	engine/sort.h
	engine/thread.h
	engine/transient.h
	engine/treeorder.h
	engine/types.h
	engine/vec.h
	engine/condition.cc
	engine/context.cc
	engine/master.cc
	engine/fileutil.cc
	engine/internalapi.cc
	engine/serial.cc
	engine/types.cc
	engine/scheduler.cc
	engine/treeorder.cc
	engine/transient.cc
	engine/flush.cc
	engine/persistent.cc
	engine/sort.cc
	engine/listener.cc
	engine/thread.cc
	engine/cache.cc
	engine/socketutil.cc
	engine/alter.cc
	engine/purge.cc
	engine/binbuffer.cc
	engine/compress.cc
	engine/coalescing.cc
	engine/io.cc
	functions/functions.h
	functions/ipaddress.h
	functions/functions.cc
	functions/ipaddress.cc
	dns/dns.h
	dns/dnscache.h
	dns/dnsconfiguration.h
	dns/dnsdefault.h
	dns/dnsnet.h
	dns/dnsserver.h
	dns/dns.cc
	dns/dnsnet.cc
	dns/dnscache.cc
	dns/dnsdefault.cc
	dns/dnsconfiguration.cc
	dns/dnsserver.cc)


ADD_DEFINITIONS(-DLOG_SUBSYSTEM_TAG="Sparrow")

MYSQL_ADD_PLUGIN(sparrow ${SPARROW_SOURCES} 
	STORAGE_ENGINE DEFAULT 
	MODULE_OUTPUT_NAME ha_sparrow 
	LINK_LIBRARIES ${SPARROW_LIBS} extra::rapidjson ext::zlib
)
#	LINK_LIBRARIES ${SPARROW_LIBS} extra::rapidjson ext::zlib mysys

IF(UNIX)
  IF(MY_COMPILER_IS_GNU)
	STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-parameter")
  ENDIF()
ELSE()
	STRING_APPEND(CMAKE_CXX_FLAGS " /wd4101")
ENDIF()

SET(SPARROW_API_SOURCES
	api/api_assert.h
	api/atomic.h
	api/bufferlist.cc
	api/bufferlist.h
	api/compress.cc
	api/compress.h
	api/cond.h
	api/memalloc.cc
	api/memalloc.h
	api/hash.h
	api/interval.h
	api/intervaltree.h
	api/ipaddress.cc
	api/ipaddress.h
	api/list.h
	api/lock.h
	api/misc.h
	api/sema.h
	api/serial.cc
	api/serial.h
	api/socketutil.cc
	api/socketutil.h
	api/spw_connection.cc
	api/spw_connection.h
	api/spw_master.cc
	api/spw_master.h
	api/spw_sparrowbuffer.cc
	api/spw_sparrowbuffer.h
	api/spw_table.cc
	api/spw_table.h
	api/spw_types.cc
	api/spw_types.h
	api/str.h
	api/thread.cc
	api/thread.h
	api/vec.h
	api/include/connection.h
	api/include/global.h
	api/include/master.h
	api/include/exception.h
	api/include/exceptwrapper.h
	api/include/sparrowbuffer.h
	api/include/table.h
	api/include/types.h)

SET(SPARROW_API_FUNCTIONS
	initialize
	createConnect
	releaseConnect
	errmsg

	CACHE INTERNAL "Functions exported by Sparrow API")

ADD_CONVENIENCE_LIBRARY(sparrow_api ${SPARROW_API_SOURCES}
	LINK_LIBRARIES ext::zlib
)
TARGET_INCLUDE_DIRECTORIES(sparrow_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/api/include)
MESSAGE(STATUS "[Sparrow] Current value of CMAKE_CURRENT_SOURCE_DIR is ${CMAKE_CURRENT_SOURCE_DIR}")

ADD_DEFINITIONS(-DSPARROW_API_EXPORTS -DHAVE_YASSL)

# BPL: The MERGE_LIBRARIES does not exist anymore. Anyway, i'm not sure this is mandatory. Let's see how the build goes without that macro call.
# MERGE_LIBRARIES(sparrowapi SHARED sparrow_api mysqlclient EXPORTS ${SPARROW_API_FUNCTIONS})

# MERGE_CONVENIENCE_LIBRARIES(mysqlclient ${LIBS_TO_MERGE}
   # COMPONENT Development
   # LINK_LIBRARIES ${LIBS_TO_LINK}
# )

# Merge several convenience libraries into one big mysqlclient and link them together into shared library.
MERGE_LIBRARIES_SHARED(sparrowapi sparrow_api mysqlclient
	EXPORTS
    ${SPARROW_API_FUNCTIONS}
)



ADD_SUBDIRECTORY(api_test)
# Missing dependency on api/include

ADD_SUBDIRECTORY(udf)

# IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
# 	SET_TARGET_PROPERTIES(sparrowapi PROPERTIES LINK_FLAGS "${LINK_FLAGS} -R $ORIGIN/../lib")
# ENDIF()
