/*
	Sparrow plugin.
*/

#include "include/mysql/plugin.h"
#include "plugin.h"
#include "hasparrow.h"
#include "../engine/coalescing.h"

using namespace Sparrow;

using std::snprintf;

#ifdef _WIN32
#define MYSQL_SYSVAR_UINT64		MYSQL_SYSVAR_ULONGLONG
#else
#define MYSQL_SYSVAR_UINT64		MYSQL_SYSVAR_ULONG
#endif

static SparrowStatus sparrowStatus;

static int show_coalescing_queue_size(MYSQL_THD thd, struct SHOW_VAR *var, char *buf)
{
	var->type= SHOW_CHAR;
	var->value= buf; // it's of SHOW_VAR_FUNC_BUFF_SIZE bytes
	snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%u", CoalescingWorker::getQueue().getSize());
	return 0;
}

static int show_disk_total(MYSQL_THD thd, struct SHOW_VAR *var, char *buf)
{
	var->type= SHOW_CHAR;
	var->value= buf; // it's of SHOW_VAR_FUNC_BUFF_SIZE bytes
	uint64_t totalFree = 0;
	uint64_t totalUsed = 0;
	uint64_t totalSize = 0;
	FileUtil::getDiskStats(totalFree, totalUsed, totalSize);
	snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%llu", static_cast<ulonglong>(totalSize));
	return 0;
}

static int show_disk_used(MYSQL_THD thd, struct SHOW_VAR *var, char *buf)
{
	var->type= SHOW_CHAR;
	var->value= buf; // it's of SHOW_VAR_FUNC_BUFF_SIZE bytes
	uint64_t totalFree = 0;
	uint64_t totalUsed = 0;
	uint64_t totalSize = 0;
	FileUtil::getDiskStats(totalFree, totalUsed, totalSize);
	snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "%llu", static_cast<ulonglong>(totalUsed));
	return 0;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif 

static SHOW_VAR sparrow_status_variables[]= {
	{ "api_active_connections", (char*) &sparrowStatus.apiActiveConnections_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "api_connections", (char*) &sparrowStatus.apiConnections_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_requests", (char*) &sparrowStatus.apiRequests_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_input_bytes", (char*) &sparrowStatus.apiInputBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_input_uncompressed_bytes", (char*) &sparrowStatus.apiInputUncompressedBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_responses", (char*) &sparrowStatus.apiResponses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_output_bytes", (char*) &sparrowStatus.apiOutputBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "api_output_uncompressed_bytes", (char*) &sparrowStatus.apiOutputUncompressedBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_read_bytes", (char*) &sparrowStatus.ioReadBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_written_bytes", (char*) &sparrowStatus.ioWrittenBytes_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_reads", (char*) &sparrowStatus.ioReads_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_writes", (char*) &sparrowStatus.ioWrites_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_opens", (char*) &sparrowStatus.ioOpens_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_closes", (char*) &sparrowStatus.ioCloses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_buffers", (char*) &sparrowStatus.ioBuffers_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "io_buffer_size", (char*) &sparrowStatus.ioBufferSize_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_nb_small", (char*) &sparrowStatus.ioNbSmall_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_nb_medium", (char*) &sparrowStatus.ioNbMedium_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "io_nb_large", (char*) &sparrowStatus.ioNbLarge_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "tuple_buffer_size", (char*) &sparrowStatus.tupleBufferSize_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "total_size", (char*) &sparrowStatus.totalSize_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "free_disk_space", (char*) &sparrowStatus.freeDiskSpace_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "file_cache_acquires", (char*) &sparrowStatus.fileCacheAcquires_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "file_cache_releases", (char*) &sparrowStatus.fileCacheReleases_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "file_cache_misses", (char*) &sparrowStatus.fileCacheMisses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "file_cache_hits", (char*) &sparrowStatus.fileCacheHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "file_cache_slow_hits", (char*) &sparrowStatus.fileCacheSlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_acquires", (char*) &sparrowStatus.blockCacheAcquires_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_releases", (char*) &sparrowStatus.blockCacheReleases_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_misses", (char*) &sparrowStatus.blockCacheMisses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_hits", (char*) &sparrowStatus.blockCacheHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_slow_hits", (char*) &sparrowStatus.blockCacheSlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level0_hits", (char*) &sparrowStatus.blockCacheLvl0Hits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level1_hits", (char*) &sparrowStatus.blockCacheLvl1Hits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level2_hits", (char*) &sparrowStatus.blockCacheLvl2Hits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level3_hits", (char*) &sparrowStatus.blockCacheLvl3Hits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level0_slow_hits", (char*) &sparrowStatus.blockCacheLvl0SlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level1_slow_hits", (char*) &sparrowStatus.blockCacheLvl1SlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level2_slow_hits", (char*) &sparrowStatus.blockCacheLvl2SlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level3_slow_hits", (char*) &sparrowStatus.blockCacheLvl3SlowHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level0_misses", (char*) &sparrowStatus.blockCacheLvl0Misses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level1_misses", (char*) &sparrowStatus.blockCacheLvl1Misses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level2_misses", (char*) &sparrowStatus.blockCacheLvl2Misses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level3_misses", (char*) &sparrowStatus.blockCacheLvl3Misses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level0_fill_ratio", (char*) &sparrowStatus.blockCacheLvl0FillRatio_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level1_fill_ratio", (char*) &sparrowStatus.blockCacheLvl1FillRatio_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level2_fill_ratio", (char*) &sparrowStatus.blockCacheLvl2FillRatio_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "block_cache_level3_fill_ratio", (char*) &sparrowStatus.blockCacheLvl3FillRatio_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "dns_caches", (char*) &sparrowStatus.dnsCaches_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_acquires", (char*) &sparrowStatus.dnsCacheAcquires_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_hits", (char*) &sparrowStatus.dnsCacheHits_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_evictions", (char*) &sparrowStatus.dnsCacheEvictions_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_entries", (char*) &sparrowStatus.dnsCacheEntries_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_pending_entries", (char*) &sparrowStatus.dnsCachePendingEntries_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_cache_size", (char*) &sparrowStatus.dnsCacheSize_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_requests", (char*) &sparrowStatus.dnsRequests_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_retries", (char*) &sparrowStatus.dnsRetries_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_responses", (char*) &sparrowStatus.dnsResponses_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_discarded_responses_1", (char*) &sparrowStatus.dnsDiscardedResponses1_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_discarded_responses_2", (char*) &sparrowStatus.dnsDiscardedResponses2_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_discarded_responses_3", (char*) &sparrowStatus.dnsDiscardedResponses3_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_discarded_responses_4", (char*) &sparrowStatus.dnsDiscardedResponses4_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_no_answer", (char*) &sparrowStatus.dnsNoAnswer_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_decoding", (char*) &sparrowStatus.dnsErrorsDecoding_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_unknown", (char*) &sparrowStatus.dnsErrorsUnknown_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_format", (char*) &sparrowStatus.dnsErrorsFormat_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_failure", (char*) &sparrowStatus.dnsErrorsFailure_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_name", (char*) &sparrowStatus.dnsErrorsName_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_not_implemented", (char*) &sparrowStatus.dnsErrorsNotImplemented_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_refused", (char*) &sparrowStatus.dnsErrorsRefused_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_yx_domain", (char*) &sparrowStatus.dnsErrorsYXDomain_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_yx_rr_set", (char*) &sparrowStatus.dnsErrorsYXRRSet_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_nx_rr_set", (char*) &sparrowStatus.dnsErrorsNXRRSet_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_not_auth", (char*) &sparrowStatus.dnsErrorsNotAuth_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "dns_errors_not_zone", (char*) &sparrowStatus.dnsErrorsNotZone_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "ddl_serial", (char*) &sparrowStatus.ddlSerial_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "worker_threads", (char*) &sparrowStatus.workerThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "flush_threads", (char*) &sparrowStatus.flushThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "dns_worker_threads", (char*) &sparrowStatus.dnsWorkerThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "writer_threads", (char*) &sparrowStatus.writerThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "api_worker_threads", (char*) &sparrowStatus.apiWorkerThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "alter_threads", (char*) &sparrowStatus.alterThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "coalescing_threads", (char*) &sparrowStatus.coalescingThreads_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "disk_total", (char*) &show_disk_total, SHOW_FUNC, SHOW_SCOPE_GLOBAL },
	{ "disk_used", (char*) &show_disk_used, SHOW_FUNC, SHOW_SCOPE_GLOBAL },
	{ "coalescing_maintask_processed", (char*) &sparrowStatus.coalescingMainTaskProcessed_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "coalescing_indextask_processed", (char*) &sparrowStatus.coalescingIndexTaskProcessed_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "coalescing_queue_size", (char*) &show_coalescing_queue_size, SHOW_FUNC, SHOW_SCOPE_GLOBAL },
	{ "flush_waits", (char*) &sparrowStatus.flushWait_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "flush_forced", (char*) &sparrowStatus.flushForced_, SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_tasks", (char*) &sparrowStatus.tasksPendingFlushTasks_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_alltasks", (char*) &sparrowStatus.tasksPendingFlushAllTasks_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_jobs", (char*) &sparrowStatus.tasksPendingFlushJobs_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_string_jobs", (char*) &sparrowStatus.tasksPendingFlushJobs_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_index_jobs", (char*) &sparrowStatus.tasksPendingIndexJobs_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_flush_write_jobs", (char*) &sparrowStatus.tasksPendingWriteJobs_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_coalescing_maintasks", (char*) &sparrowStatus.tasksPendingCoalescingMainTasks_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_coalescing_indextasks", (char*) &sparrowStatus.tasksPendingCoalescingIndexTasks_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ "tasks_pending_dns_tasks", (char*) &sparrowStatus.tasksPendingDnsTasks_, SHOW_INT, SHOW_SCOPE_GLOBAL },
	{ NullS, NullS, SHOW_INT, SHOW_SCOPE_GLOBAL }
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

static int show_sparrow_vars(THD *thd, SHOW_VAR *var, char *buff) {
	sparrowStatus = SparrowStatus::get();
	var->type= SHOW_ARRAY;
	var->value= (char *) &sparrow_status_variables;
	var->scope = SHOW_SCOPE_GLOBAL;
	return 0;
}

static SHOW_VAR sparrow_status_variables_export[]= {
	{ "sparrow", (char*) &show_sparrow_vars, SHOW_FUNC, SHOW_SCOPE_GLOBAL },
	{ NullS, NullS,	SHOW_LONG, SHOW_SCOPE_GLOBAL }
};

uint sparrow_open_files;
uint sparrow_cache_block_size, sparrow_small_read_block_size, sparrow_medium_read_block_size, sparrow_large_read_block_size;
uint sparrow_write_block_size, sparrow_transfer_block_size;
int sparrow_socket_sndbuf_size, sparrow_socket_rcvbuf_size;
uint sparrow_idle_thread_timeout;
uint sparrow_default_max_lifetime;
uint sparrow_index_cost_percentage;
uint64_t sparrow_max_disk_size;
bool sparrow_async_io;
bool sparrow_coalescing, sparrow_purge_constantly;
uint64_t sparrow_purge_security_margin;
uint sparrow_max_flush_threads, sparrow_max_worker_threads, sparrow_max_writer_threads, sparrow_max_dns_worker_threads, sparrow_max_alter_threads, sparrow_max_coalescing_threads, sparrow_max_api_worker_threads;
uint sparrow_flush_interval;
uint64_t sparrow_direct_insertion_threshold;
uint64_t sparrow_max_tuple_buffer_size;
uint64_t sparrow_default_string_optimization_size;
uint sparrow_tuple_buffer_threshold;
uint sparrow_listener_port;
char* sparrow_listener_address;
uint sparrow_max_connections;
ulong sparrow_incompatible_table;
uint64_t sparrow_cache0_size, sparrow_cache1_size, sparrow_cache2_size, sparrow_cache3_size;
uint sparrow_disk_sector_size;
uint64_t sparrow_max_dns_cache_size;
uint sparrow_dns_timeout;
uint sparrow_dns_retries;
char* sparrow_filesystems;
char* sparrow_coalescing_filesystems;
bool sparrow_column_optimisation;
uint sparrow_column_optimisation_lvl;
bool sparrow_quick_shutdown;
bool sparrow_auto_partition_repair;
bool sparrow_disable_purge;
bool sparrow_log_purge_activity;

const uint32_t SPARROW_VERSION = 0x0100;	/* 1.0 */

static MYSQL_SYSVAR_UINT(open_files,
	sparrow_open_files,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Maximum number of files opened simultaneously in Sparrow.",
	0, 0, 300, 10, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(default_max_lifetime,
	sparrow_default_max_lifetime,
	PLUGIN_VAR_RQCMDARG,
	"Default maximum lifetime of data in a table, in days. When this lifetime is reached, older data are purged.",
	0, 0, 365, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(index_cost_percentage,
	sparrow_index_cost_percentage,
	PLUGIN_VAR_RQCMDARG,
	"Percentage applied to the number of records in range, in order to favor index scans over table scans. Default is 50%: twice less records in range than computed.",
	0, 0, 50, 0, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT64(max_disk_size,
	sparrow_max_disk_size,
	PLUGIN_VAR_RQCMDARG,
	"Maximum disk size used by all Sparrow tables. If 0, all disk space except a security margin of 1 GB per file system may be used.",
	0, 0, 0, 0, ULONG_MAX, 0);

static MYSQL_SYSVAR_UINT(small_read_block_size,
	sparrow_small_read_block_size,
	PLUGIN_VAR_RQCMDARG,
	"Block size used when reading e.g. data files during an index scan.",
	0, 0, 512*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_UINT(medium_read_block_size,
	sparrow_medium_read_block_size,
	PLUGIN_VAR_RQCMDARG,
	"Block size used when reading e.g. master files.",
	0, 0, 1024*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_UINT(large_read_block_size,
	sparrow_large_read_block_size,
	PLUGIN_VAR_RQCMDARG,
	"Block size used when reading e.g. index records during an index scan or data records during a full scan.",
	0, 0, 2048*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_UINT(write_block_size,
	sparrow_write_block_size,
	PLUGIN_VAR_RQCMDARG,
	"Block size used when writing to files.",
	0, 0, 64*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_UINT(transfer_block_size,
	sparrow_transfer_block_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Block size used when reading data from the network.",
	0, 0, 1024*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_INT(socket_sndbuf_size,
	sparrow_socket_sndbuf_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of the socket buffer used to send data.",
	0, 0, 2048, 512, INT_MAX, 0);

static MYSQL_SYSVAR_INT(socket_rcvbuf_size,
	sparrow_socket_rcvbuf_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of the socket buffer used to receive data.",
	0, 0, 1024*1024, 512, INT_MAX, 0);

static MYSQL_SYSVAR_UINT(cache_block_size,
	sparrow_cache_block_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Block size used when caching data.",
	0, 0, 4*1024, 512, UINT_MAX, 512);

static MYSQL_SYSVAR_UINT64(cache0_size,
	sparrow_cache0_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of level 0 cache. This cache level contains data read recently but not yet used.",
	0, 0, 16*1024*1024, 1024*1024, ULONG_MAX, 1024*1024);

static MYSQL_SYSVAR_UINT64(cache1_size,
	sparrow_cache1_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of level 1 cache. This cache level contains meta data and index records from recent queries.",
	0, 0, 64*1024*1024, 1024*1024, ULONG_MAX, 1024*1024);

static MYSQL_SYSVAR_UINT64(cache2_size,
	sparrow_cache2_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of level 2 cache. This cache level contains data records from recent queries.",
	0, 0, 64*1024*1024, 1024*1024, ULONG_MAX, 1024*1024);

static MYSQL_SYSVAR_UINT64(cache3_size,
	sparrow_cache3_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Size of level 3 cache. This cache level contains data records from recent inserts.",
	0, 0, 64*1024*1024, 1024*1024, ULONG_MAX, 1024*1024);

static MYSQL_SYSVAR_UINT(idle_thread_timeout,
	sparrow_idle_thread_timeout,
	PLUGIN_VAR_OPCMDARG,
	"Idle thread timeout, in milliseconds. When an idle thread times out, it is destroyed.",
	0, 0, 300000, 1000, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_flush_threads,
	sparrow_max_flush_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of flush threads.",
	0, 0, 4, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_worker_threads,
	sparrow_max_worker_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of worker threads.",
	0, 0, 4, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_writer_threads,
	sparrow_max_writer_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of writer threads.",
	0, 0, 1, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_dns_worker_threads,
	sparrow_max_dns_worker_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of worker threads for DNS resolution.",
	0, 0, 2, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_alter_threads,
	sparrow_max_alter_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of threads used for online table modifications.",
	0, 0, 1, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_BOOL(async_io,
	sparrow_async_io,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"If true, use asynchronous I/O. This is enabled by default.",
	0, 0, true);

static MYSQL_SYSVAR_BOOL(coalescing,
	sparrow_coalescing,
	PLUGIN_VAR_RQCMDARG,
	"If false, partition coalescing is disabled. It is enabled by default.",
	0, 0, true);

static MYSQL_SYSVAR_BOOL(purge_constantly,
	sparrow_purge_constantly,
	PLUGIN_VAR_RQCMDARG,
	"If true, the purge process compares the partition timestamps to the current time instead of the most recent inserted timestamp. Therefore, if the client application stops inserting data into a table, this data from this table will continue to be purge regularly. It is disabled by default.",
	0, 0, false);

static MYSQL_SYSVAR_UINT64(purge_security_margin,
	sparrow_purge_security_margin,
	PLUGIN_VAR_RQCMDARG,
	"Sets the amount of free space to leave on each file system.",
	0, 0, 1024ULL*1024*1024, 1024, ULONG_MAX, 1024);

static MYSQL_SYSVAR_UINT(max_coalescing_threads,
	sparrow_max_coalescing_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of threads used for partition coalescing.",
	0, 0, 1, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(max_api_worker_threads,
	sparrow_max_api_worker_threads,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of API worker threads.",
	0, 0, 10, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT64(direct_insertion_threshold,
	sparrow_direct_insertion_threshold,
	PLUGIN_VAR_RQCMDARG,
	"Insertion size threshold : If the inserted data are above this threshold, the data is directly inserted to the engine without using the context buffer",
	0, 0, 512*1024, 1024, ULONG_MAX, 1024);

static MYSQL_SYSVAR_UINT64(max_tuple_buffer_size,
	sparrow_max_tuple_buffer_size,
	PLUGIN_VAR_RQCMDARG,
	"Maximum size of the tuple buffer. When this buffer is full, insertions are blocked until a flush occurs.",
	0, 0, 64*1024*1024, 1024*1024, ULONG_MAX, 1024*1024);

static MYSQL_SYSVAR_UINT64(default_string_optimization_size,
	sparrow_default_string_optimization_size,
	PLUGIN_VAR_RQCMDARG,
	"Default amount of data read from the string file when optimizing transient strings. Strings already present in the file will not be flushed.",
	0, 0, 16*1024*1024, 0, ULONG_MAX, 0);

static MYSQL_SYSVAR_UINT(tuple_buffer_threshold,
	sparrow_tuple_buffer_threshold,
	PLUGIN_VAR_RQCMDARG,
	"Threshold, in percentage of sparrow_max_tuple_buffer_size, above which data are flushed to disk.",
	0, 0, 50, 1, 100, 0);

#ifdef NDEBUG
#define MIN_SPARROW_FLUSH_INTERVAL 5
#else
#define MIN_SPARROW_FLUSH_INTERVAL 0
#endif
static MYSQL_SYSVAR_UINT(flush_interval,
	sparrow_flush_interval,
	PLUGIN_VAR_RQCMDARG,
	"Interval, in seconds, between data flushes. Data may be written more often if the maximum tuple buffer size is reached before this time interval elapses.",
	0, 0, 300, MIN_SPARROW_FLUSH_INTERVAL, 3600, 0);

static MYSQL_SYSVAR_UINT(listener_port,
	sparrow_listener_port,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"TCP port on which Sparrow binds its listener socket.",
	0, 0, 11000, 1, 65535, 0);

static MYSQL_SYSVAR_STR(listener_address,
	sparrow_listener_address,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Local address on which Sparrow binds its listener socket.",
	0, 0, 0);

static MYSQL_SYSVAR_UINT(max_connections,
	sparrow_max_connections,
	PLUGIN_VAR_RQCMDARG,
	"Maximum number of simultaneous connections on Sparrow.",
	0, 0, 10, 1, UINT_MAX, 0);

const char* sparrowIncompatibleTableNames[] = { "drop", "rename", NullS };

TYPELIB sparrowIncompatibleTableTypelib = {
	array_elements(sparrowIncompatibleTableNames) - 1,
	"sparrowIncompatibleTableTypelib",
	sparrowIncompatibleTableNames,
	0
};

static MYSQL_SYSVAR_ENUM(incompatible_table,
	sparrow_incompatible_table,
	PLUGIN_VAR_RQCMDARG,
	"Action taken when an incompatible Sparrow table is detected.",
	0,
	0,
	0,
	&sparrowIncompatibleTableTypelib);

static MYSQL_SYSVAR_UINT(disk_sector_size,
	sparrow_disk_sector_size,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Unix only: disk block size of the partition where the database resides.",
	0, 0, 512, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT64(max_dns_cache_size,
	sparrow_max_dns_cache_size,
	PLUGIN_VAR_RQCMDARG,
	"Maximum size of the DNS cache.",
	0, 0, 64*1024*1024, 0, ULONG_MAX, 1024);

static MYSQL_SYSVAR_UINT(dns_timeout,
	sparrow_dns_timeout,
	PLUGIN_VAR_RQCMDARG,
	"Default DNS query timeout, in milliseconds.",
	0, 0, 200, 1, 1000, 0);

static MYSQL_SYSVAR_UINT(dns_retries,
	sparrow_dns_retries,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Default maximum number of retries when a query times out. If multiple servers are available for the query, the retry is performed on the next server.",
	0, 0, 2, 0, 10, 0);

static MYSQL_SYSVAR_STR(filesystems,
	sparrow_filesystems,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Comma separated list of additional file systems where Sparrow will store its data and index files. Caution: removing a file system from this list requires a database re-initialization.",
	0, 0, 0);

static MYSQL_SYSVAR_STR(coalescing_filesystems,
	sparrow_coalescing_filesystems,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Comma separated list of additional file systems where Sparrow will store index files before coalescing. To be efficient, this file system must be backed by fast HDDs or SSDs. Caution: removing a file system from this list requires a database re-initialization.",
	0, 0, 0);

static MYSQL_SYSVAR_BOOL(column_optimisation,
	sparrow_column_optimisation,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"If true, use nullptr column optimization. This is enabled by default.",
	0, 0, true);

static MYSQL_SYSVAR_UINT(column_optimisation_lvl,
	sparrow_column_optimisation_lvl,
	PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
	"Sparrow automatically removes from partitions columns that contains only nullptr values. But partitions with different sets of nullptr columns cannot be coalesced together. This can thus generate an increase in the number of partitions. To limit that increase, you can change this parameter. If set to 0, all the nullptr columns will be optimized, but it may generate many partitions. If set to high value (example: 10), as many nullptr columns won't be optimized, but a lot less partitions will be generated. Default value is 2.",
	0, 0, 2, 0, 10, 0);

static MYSQL_SYSVAR_BOOL(quick_shutdown,
	sparrow_quick_shutdown,
	PLUGIN_VAR_RQCMDARG,
	"If true, data in memory are not written to disk upon shutdown to save time. Setting it to false increases shutdown time significantly and, if process is killed during that time, files may get corrupted. This is enabled by default.",
	0, 0, true);

static MYSQL_SYSVAR_BOOL(auto_partition_repair,
	sparrow_auto_partition_repair,
	PLUGIN_VAR_RQCMDARG,
	"If true, Sparrow will automatically try to repair corrupted partitions, and delete them if the repair fails. This is enabled by default.",
	0, 0, true);

static MYSQL_SYSVAR_BOOL(disable_purge,
	sparrow_disable_purge,
	PLUGIN_VAR_RQCMDARG,
	"If true, automatic partition purging is disabled. This is for debugging purpose and should ony be used by dev team. This is false by default.",
	0, 0, false);

static MYSQL_SYSVAR_BOOL(log_purge_activity,
	sparrow_log_purge_activity,
	PLUGIN_VAR_RQCMDARG,
	"If true, a message will be logged if disk space is too low and forces the purge module to delete old data partitions that have not yet reached their max lifetime to make space for newly inserted data. This is true by default.",
	0, 0, true);


static struct SYS_VAR* sparrow_system_variables[] = {
	MYSQL_SYSVAR(open_files),
	MYSQL_SYSVAR(default_max_lifetime),
	MYSQL_SYSVAR(index_cost_percentage),
	MYSQL_SYSVAR(max_disk_size),
	MYSQL_SYSVAR(small_read_block_size),
	MYSQL_SYSVAR(medium_read_block_size),
	MYSQL_SYSVAR(large_read_block_size),
	MYSQL_SYSVAR(write_block_size),
	MYSQL_SYSVAR(transfer_block_size),
	MYSQL_SYSVAR(socket_sndbuf_size),
	MYSQL_SYSVAR(socket_rcvbuf_size),
	MYSQL_SYSVAR(cache_block_size),
	MYSQL_SYSVAR(cache0_size),
	MYSQL_SYSVAR(cache1_size),
	MYSQL_SYSVAR(cache2_size),
	MYSQL_SYSVAR(cache3_size),
	MYSQL_SYSVAR(idle_thread_timeout),
	MYSQL_SYSVAR(max_flush_threads),
	MYSQL_SYSVAR(max_worker_threads),
	MYSQL_SYSVAR(max_api_worker_threads),
	MYSQL_SYSVAR(max_writer_threads),
	MYSQL_SYSVAR(max_dns_worker_threads),
	MYSQL_SYSVAR(max_alter_threads),
	MYSQL_SYSVAR(async_io),
	MYSQL_SYSVAR(coalescing),
	MYSQL_SYSVAR(purge_constantly),
	MYSQL_SYSVAR(purge_security_margin),
	MYSQL_SYSVAR(max_coalescing_threads),
	MYSQL_SYSVAR(max_tuple_buffer_size),
	MYSQL_SYSVAR(direct_insertion_threshold),
	MYSQL_SYSVAR(default_string_optimization_size),
	MYSQL_SYSVAR(tuple_buffer_threshold),
	MYSQL_SYSVAR(flush_interval),
	MYSQL_SYSVAR(listener_port),
	MYSQL_SYSVAR(listener_address),
	MYSQL_SYSVAR(max_connections),
	MYSQL_SYSVAR(incompatible_table),
	MYSQL_SYSVAR(disk_sector_size),
	MYSQL_SYSVAR(max_dns_cache_size),
	MYSQL_SYSVAR(dns_timeout),
	MYSQL_SYSVAR(dns_retries),
	MYSQL_SYSVAR(filesystems),
	MYSQL_SYSVAR(coalescing_filesystems),
	MYSQL_SYSVAR(column_optimisation),
	MYSQL_SYSVAR(column_optimisation_lvl),
	MYSQL_SYSVAR(quick_shutdown),
	MYSQL_SYSVAR(auto_partition_repair),
	MYSQL_SYSVAR(disable_purge),
	MYSQL_SYSVAR(log_purge_activity),
	0
};

// Information schema plugins for Sparrow.
struct st_mysql_information_schema sparrow_is_info = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };
struct st_mysql_plugin sparrow_tables = {
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&sparrow_is_info,
	SPARROW_IS_TABLES_NAME,
	SPARROW_AUTH,
	SPARROW_IS_TABLES_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initializeISTables,
	nullptr,
	SparrowHandler::deinitializeISTables,
	SPARROW_VERSION,
	nullptr,
	nullptr,
	nullptr,
	0 
};

struct st_mysql_plugin sparrow_columns = {
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&sparrow_is_info,
	SPARROW_IS_COLUMNS_NAME,
	SPARROW_AUTH,
	SPARROW_IS_COLUMNS_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initializeISColumns,
	nullptr,
	SparrowHandler::deinitializeISColumns,
	SPARROW_VERSION,
	nullptr,
	nullptr,
	nullptr,
	0 
};

struct st_mysql_plugin sparrow_indexes = {
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&sparrow_is_info,
	SPARROW_IS_INDEXES_NAME,
	SPARROW_AUTH,
	SPARROW_IS_INDEXES_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initializeISIndexes,
	nullptr,
	SparrowHandler::deinitializeISIndexes,
	SPARROW_VERSION,
	nullptr,
	nullptr,
	nullptr,
	0 
};

struct st_mysql_plugin sparrow_alterations = {
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&sparrow_is_info,
	SPARROW_IS_ALTERATIONS_NAME,
	SPARROW_AUTH,
	SPARROW_IS_ALTERATIONS_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initializeISAlterations,
	nullptr,
	SparrowHandler::deinitializeISAlterations,
	SPARROW_VERSION,
	nullptr,
	nullptr,
	nullptr,
	0 
};

struct st_mysql_plugin sparrow_partitions = {
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&sparrow_is_info,
	SPARROW_IS_PARTITIONS_NAME,
	SPARROW_AUTH,
	SPARROW_IS_PARTITIONS_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initializeISPartitions,
	nullptr,
	SparrowHandler::deinitializeISPartitions,
	SPARROW_VERSION,
	nullptr,
	nullptr,
	nullptr,
	0 
};

// The Sparrow storage engine plugin.
struct st_mysql_storage_engine sparrow_storage_engine = { MYSQL_HANDLERTON_INTERFACE_VERSION };
mysql_declare_plugin(sparrow) {
	MYSQL_STORAGE_ENGINE_PLUGIN,
	&sparrow_storage_engine,
	SPARROW_ENGINE_NAME,
	SPARROW_AUTH,
	SPARROW_ENGINE_DESC,
	PLUGIN_LICENSE_PROPRIETARY,
	SparrowHandler::initialize,			/* Plugin init. */
	nullptr,							/* Plugin uninstall. Not necessary.  */
	SparrowHandler::deinitialize,		/* Plugin deinit. */
	SPARROW_VERSION,
	sparrow_status_variables_export,	/* Status variables. */
	sparrow_system_variables,			/* System variables. */
	nullptr,							/* Config options, reserved for future dependency checking. */
	0
},
sparrow_tables,
sparrow_columns,
sparrow_indexes,
sparrow_alterations,
sparrow_partitions
mysql_declare_plugin_end;
