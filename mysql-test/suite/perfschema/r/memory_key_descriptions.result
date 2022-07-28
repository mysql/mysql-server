SELECT NAME, DOCUMENTATION FROM performance_schema.setup_instruments
WHERE NAME LIKE 'memory/sql/%'
                        AND DOCUMENTATION IS NOT NULL
ORDER BY NAME;
NAME	DOCUMENTATION
memory/sql/dd::column_statistics	Column statistics histograms allocated.
memory/sql/dd::default_values	Temporary buffer for preparing column default values.
memory/sql/dd::import	File name handling while importing MyISAM tables.
memory/sql/dd::infrastructure	Infrastructure of the data dictionary structures.
memory/sql/dd::objects	Memory occupied by the data dictionary objects.
memory/sql/dd::String_type	Character strings used by data dictionary objects.
memory/sql/display_table_locks	Debug utility.
memory/sql/errmsgs::handler	Handler error messages (HA_ERR_...).
memory/sql/errmsgs::server	In-memory representation of server error messages.
memory/sql/Event_basic::mem_root	Event base class with root used for definiton etc.
memory/sql/Event_queue_element_for_exec::names	Copy of schema- and event name in exec queue element.
memory/sql/Event_scheduler::scheduler_param	Infrastructure of the priority queue of events.
memory/sql/handlerton::objects	Handlerton objects.
memory/sql/help	Temporary memroot used to print help texts as part of usage description.
memory/sql/host_cache::hostname	Hostname keys in the host_cache map.
memory/sql/Locked_tables_list::m_locked_tables_root	Memroot for list of locked tables.
memory/sql/log_error::loaded_services	Memory allocated for duplicate log events.
memory/sql/log_error::stack	Log events for the error log.
memory/sql/LOG::file_name	File name of slow log and general log.
memory/sql/MDL_context::acquire_locks	Buffer for sorting lock requests.
memory/sql/MDL_context::backup_manager	MDL for prepared XA trans with disconnected client.
memory/sql/MYSQL_LOCK	Table locks per session.
memory/sql/NET::buff	Buffer in the client protocol communications layer.
memory/sql/NET::compress_packet	Buffer used when compressing a packet.
memory/sql/Partition::admin	Buffer for printing messages into the client protocol.
memory/sql/Partition::prune_exec	Mem root used temporarily while pruning partitions.
memory/sql/Partition::share	Partition name and auto increment mutex.
memory/sql/Partition::sort_buffer	Record buffer for a partition.
memory/sql/Partition::syntax_buffer	Buffer used for formatting the partition expression.
memory/sql/Persisted_variables::memroot	Memory allocated to process persisted variables during server start-up and plugin/component initialization.
memory/sql/Persisted_variables::unordered_map	Memory allocated for in-memory maps for persisted variables
memory/sql/Persisted_variables::unordered_set	Memory allocated for in-memory sets for persisted variables
memory/sql/Prepared_statement::infrastructure	Map infrastructure for prepared statements per session.
memory/sql/Prepared_statement::main_mem_root	Mem root for each prepared statement for items etc.
memory/sql/rm_db::mdl_reqs_root	Mem root for allocating MDL requests while dropping datbase.
memory/sql/rm_table::foreach_root	Mem root for temporary objects allocated while dropping tables or the whole database.
memory/sql/servers_cache	Cache infrastructure and mem root for servers cache.
memory/sql/Shared_memory_name	Communication through shared memory (windows).
memory/sql/sp_head::call_mem_root	Mem root for objects with same life time as stored program call.
memory/sql/sp_head::execute_mem_root	Mem root per instruction.
memory/sql/sp_head::main_mem_root	Mem root for parsing and representation of stored programs.
memory/sql/ST_SCHEMA_TABLE	Structure describing an information schema table implemented by a plugin.
memory/sql/TABLE	Memory used by TABLE objects and their mem root.
memory/sql/TABLE_SHARE::mem_root	Cache infrastructure and individual table shares.
memory/sql/TC_LOG_MMAP::pages	In-memory transaction coordinator log.
memory/sql/thd_timer	Thread timer object.
memory/sql/THD::db	Name of currently used schema.
memory/sql/THD::debug_sync_action	Debug sync actions to perform per thread.
memory/sql/THD::debug_sync_control	Structure to control debug sync per thread.
memory/sql/THD::handler_tables_hash	Hash map of tables used by HANDLER statements.
memory/sql/THD::main_mem_root	Main mem root used for e.g. the query arena.
memory/sql/THD::sp_cache	Per session cache for stored programs.
memory/sql/THD::transactions::mem_root	Transaction context information per session.
memory/sql/THD::variables	Per session copy of global dynamic variables.
memory/sql/tz_storage	Shared time zone data.
memory/sql/udf_mem	Shared structure of UDFs.
memory/sql/user_conn	Objects describing user connections.
memory/sql/User_level_lock	Per session storage of user level locks.
memory/sql/XA::recovered_transactions	List infrastructure for recovered XA transactions.
memory/sql/XA::transaction_contexts	Shared cache of XA transaction contexts.
SET @rows = (SELECT COUNT(NAME) FROM performance_schema.setup_instruments
WHERE NAME LIKE 'memory/sql/%'
                        AND DOCUMENTATION IS NOT NULL
ORDER BY NAME);
include/assert.inc [The number of documented P_S memory keys in the SQL category is as expected.]
