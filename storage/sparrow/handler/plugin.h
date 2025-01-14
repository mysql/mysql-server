/*
	Sparrow plugin.
*/

#ifndef _handler_plugin_h_
#define _handler_plugin_h_

//#include <sql_priv.h>
#include "sql/query_options.h"		// For mysqld options.
#include "sql/sql_plugin.h"
#include "mysys_err.h"

#define SPARROW_ENGINE_NAME					"SPARROW"
#define SPARROW_ENGINE_DESC					"MySQL Storage Engine for InfoVista"
#define SPARROW_AUTH						"InfoVista S.A."

#define SPARROW_IS_TABLES_NAME				"SPARROW_TABLES"
#define SPARROW_IS_TABLES_DESC				"Information about Sparrow tables"

#define SPARROW_IS_COLUMNS_NAME				"SPARROW_COLUMNS"
#define SPARROW_IS_COLUMNS_DESC				"Information about columns in Sparrow tables"

#define SPARROW_IS_INDEXES_NAME				"SPARROW_INDEXES"
#define SPARROW_IS_INDEXES_DESC				"Information about indexes on Sparrow tables"

#define SPARROW_IS_ALTERATIONS_NAME			"SPARROW_ALTERATIONS"
#define SPARROW_IS_ALTERATIONS_DESC			"Information about on going alterations on Sparrow tables"

#define SPARROW_IS_PARTITIONS_NAME			"SPARROW_PARTITIONS"
#define SPARROW_IS_PARTITIONS_DESC			"Information about partitions in Sparrow tables"

extern uint sparrow_open_files;
extern uint sparrow_cache_block_size, sparrow_small_read_block_size, sparrow_medium_read_block_size, sparrow_large_read_block_size;
extern uint sparrow_write_block_size, sparrow_transfer_block_size;
extern int sparrow_socket_sndbuf_size, sparrow_socket_rcvbuf_size;
extern uint sparrow_default_time_period;
extern uint sparrow_default_max_lifetime;
extern uint sparrow_index_cost_percentage;
extern uint64_t sparrow_max_disk_size;
extern bool sparrow_async_io;
extern bool sparrow_coalescing, sparrow_purge_constantly;
extern uint64_t sparrow_purge_security_margin;
extern uint sparrow_max_flush_threads, sparrow_max_worker_threads, sparrow_max_writer_threads, sparrow_max_dns_worker_threads, sparrow_max_alter_threads, sparrow_max_coalescing_threads, sparrow_max_api_worker_threads;
extern uint sparrow_flush_interval;
extern uint64_t sparrow_max_tuple_buffer_size;
extern uint64_t sparrow_default_string_optimization_size;
extern uint64_t sparrow_direct_insertion_threshold;
extern uint sparrow_tuple_buffer_threshold;
extern uint sparrow_listener_port;
extern char* sparrow_listener_address;
extern uint sparrow_max_connections;
extern ulong sparrow_incompatible_table;
extern uint64_t sparrow_cache0_size, sparrow_cache1_size, sparrow_cache2_size, sparrow_cache3_size;
extern uint sparrow_disk_sector_size;
extern uint64_t sparrow_max_dns_cache_size;
extern uint sparrow_dns_timeout;
extern uint sparrow_dns_retries;
extern char* sparrow_filesystems;
extern char* sparrow_coalescing_filesystems;
extern bool sparrow_column_optimisation;
extern uint sparrow_column_optimisation_lvl;
extern bool sparrow_quick_shutdown;
extern bool sparrow_auto_partition_repair;
extern bool sparrow_disable_purge;
extern bool sparrow_log_purge_activity;

extern const uint32_t SPARROW_VERSION;

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowStatus
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SparrowStatus {
private:

	static SparrowStatus status_;

public:

	// API stats.
	volatile uint32_t apiActiveConnections_;
	volatile uint64_t apiConnections_;
	volatile uint64_t apiRequests_;
	volatile uint64_t apiInputBytes_;
	volatile uint64_t apiInputUncompressedBytes_;
	volatile uint64_t apiResponses_;
	volatile uint64_t apiOutputBytes_;
	volatile uint64_t apiOutputUncompressedBytes_;

	// IO stats
	volatile uint64_t ioReadBytes_;
	volatile uint64_t ioWrittenBytes_;
	volatile uint64_t ioReads_;
	volatile uint64_t ioWrites_;
	volatile uint64_t ioOpens_;
	volatile uint64_t ioCloses_;
	volatile uint32_t ioBuffers_;
	volatile uint64_t ioBufferSize_;
	volatile uint64_t ioNbSmall_;
	volatile uint64_t ioNbMedium_;
	volatile uint64_t ioNbLarge_;

	// Sizes.
	volatile uint64_t tupleBufferSize_;
	volatile uint64_t totalSize_;
	volatile uint64_t freeDiskSpace_;

	// Stats for file cache (see CacheStat class).
	volatile uint64_t fileCacheAcquires_;
	volatile uint64_t fileCacheReleases_;
	volatile uint64_t fileCacheMisses_;
	volatile uint64_t fileCacheHits_;
	volatile uint64_t fileCacheSlowHits_;

	// Stats for block cache (see CacheStat class).
	volatile uint64_t blockCacheAcquires_;
	volatile uint64_t blockCacheReleases_;
	volatile uint64_t blockCacheMisses_;
	volatile uint64_t blockCacheHits_;
	volatile uint64_t blockCacheSlowHits_;
	volatile uint64_t blockCacheLvl0Hits_;
	volatile uint64_t blockCacheLvl1Hits_;
	volatile uint64_t blockCacheLvl2Hits_;
	volatile uint64_t blockCacheLvl3Hits_;
	volatile uint64_t blockCacheLvl0SlowHits_;
	volatile uint64_t blockCacheLvl1SlowHits_;
	volatile uint64_t blockCacheLvl2SlowHits_;
	volatile uint64_t blockCacheLvl3SlowHits_;
	volatile uint64_t blockCacheLvl0Misses_;
	volatile uint64_t blockCacheLvl1Misses_;
	volatile uint64_t blockCacheLvl2Misses_;
	volatile uint64_t blockCacheLvl3Misses_;
	volatile uint32_t blockCacheLvl0FillRatio_;
	volatile uint32_t blockCacheLvl1FillRatio_;
	volatile uint32_t blockCacheLvl2FillRatio_;
	volatile uint32_t blockCacheLvl3FillRatio_;

	// Stats for DNS.
	volatile uint32_t dnsCaches_;
	volatile uint64_t dnsCacheAcquires_;
	volatile uint64_t dnsCacheHits_;
	volatile uint64_t dnsCacheEvictions_;
	volatile uint64_t dnsCacheEntries_;
	volatile uint64_t dnsCachePendingEntries_;
	volatile uint64_t dnsCacheSize_;
	volatile uint64_t dnsRequests_;
	volatile uint64_t dnsRetries_;
	volatile uint64_t dnsResponses_;
	volatile uint64_t dnsDiscardedResponses1_;
	volatile uint64_t dnsDiscardedResponses2_;
	volatile uint64_t dnsDiscardedResponses3_;
	volatile uint64_t dnsDiscardedResponses4_;
	volatile uint64_t dnsNoAnswer_;
	volatile uint64_t dnsErrorsDecoding_;
	volatile uint64_t dnsErrorsUnknown_;
	volatile uint64_t dnsErrorsFormat_;
	volatile uint64_t dnsErrorsFailure_;
	volatile uint64_t dnsErrorsName_;
	volatile uint64_t dnsErrorsNotImplemented_;
	volatile uint64_t dnsErrorsRefused_;
	volatile uint64_t dnsErrorsYXDomain_;
	volatile uint64_t dnsErrorsYXRRSet_;
	volatile uint64_t dnsErrorsNXRRSet_;
	volatile uint64_t dnsErrorsNotAuth_;
	volatile uint64_t dnsErrorsNotZone_;

	// DDL serial counter.
	volatile uint32_t ddlSerial_;

	// Thread counts.
	volatile uint32_t workerThreads_;
	volatile uint32_t flushThreads_;
	volatile uint32_t dnsWorkerThreads_;
	volatile uint32_t writerThreads_;
	volatile uint32_t apiWorkerThreads_;
	volatile uint32_t alterThreads_;
	volatile uint32_t coalescingThreads_;

	volatile uint64_t coalescingIndexTaskProcessed_;
	volatile uint64_t coalescingMainTaskProcessed_;

	// Flushs
	volatile uint64_t	flushWait_;
	volatile uint64_t flushForced_;

	// Pending jobs
	volatile uint32_t	tasksPendingFlushTasks_;
	volatile uint32_t	tasksPendingFlushAllTasks_;
	volatile uint32_t	tasksPendingFlushJobs_;
	volatile uint32_t	tasksPendingStringJobs_;
	volatile uint32_t	tasksPendingIndexJobs_;
	volatile uint32_t	tasksPendingWriteJobs_;
	volatile uint32_t tasksPendingCoalescingMainTasks_;
	volatile uint32_t tasksPendingCoalescingIndexTasks_;
	volatile uint32_t tasksPendingDnsTasks_;

	// TODO error stats

public:

	SparrowStatus() {
		memset(this, 0, sizeof(*this));
	}

	static SparrowStatus& get() {
		return status_;
	}
};

}

#endif /* #ifndef _handler_plugin_h_ */
