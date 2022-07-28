/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/psi_memory_key.h"

#include "my_psi_config.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_memory_bits.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/psi_memory.h"
#include "template_utils.h"

/*
  MAINTAINER: Please keep this list in order, to limit merge collisions.
*/

PSI_memory_key key_memory_DD_cache_infrastructure;
PSI_memory_key key_memory_DD_column_statistics;
PSI_memory_key key_memory_DD_default_values;
PSI_memory_key key_memory_DD_import;
PSI_memory_key key_memory_DD_objects;
PSI_memory_key key_memory_DD_String_type;
PSI_memory_key key_memory_Event_queue_element_for_exec_names;
PSI_memory_key key_memory_Event_scheduler_scheduler_param;
PSI_memory_key key_memory_File_query_log_name;
PSI_memory_key key_memory_Filesort_info_merge;
PSI_memory_key key_memory_Filesort_info_record_pointers;
PSI_memory_key key_memory_Geometry_objects_data;
PSI_memory_key key_memory_Gis_read_stream_err_msg;
PSI_memory_key key_memory_Gtid_state_to_string;
PSI_memory_key key_memory_HASH_ROW_ENTRY;
PSI_memory_key key_memory_JOIN_CACHE;
PSI_memory_key key_memory_JSON;
PSI_memory_key key_memory_LOG_POS_COORD;
PSI_memory_key key_memory_LOG_name;
PSI_memory_key key_memory_MPVIO_EXT_auth_info;
PSI_memory_key key_memory_MYSQL_BIN_LOG_basename;
PSI_memory_key key_memory_MYSQL_BIN_LOG_index;
PSI_memory_key key_memory_MYSQL_LOCK;
PSI_memory_key key_memory_MYSQL_LOG_name;
PSI_memory_key key_memory_MYSQL_RELAY_LOG_basename;
PSI_memory_key key_memory_MYSQL_RELAY_LOG_index;
PSI_memory_key key_memory_Mutex_cond_array_Mutex_cond;
PSI_memory_key key_memory_NET_buff;
PSI_memory_key key_memory_NET_compress_packet;
PSI_memory_key key_memory_Owned_gtids_sidno_to_hash;
PSI_memory_key key_memory_Owned_gtids_to_string;
PSI_memory_key key_memory_PROFILE;
PSI_memory_key key_memory_IndexRangeScanIterator_mrr_buf_desc;
PSI_memory_key key_memory_READ_INFO;
PSI_memory_key key_memory_READ_RECORD_cache;
PSI_memory_key key_memory_xa_recovered_transactions;
PSI_memory_key key_memory_Row_data_memory_memory;
PSI_memory_key key_memory_Rpl_info_file_buffer;
PSI_memory_key key_memory_Rpl_info_table;
PSI_memory_key key_memory_REPLICA_INFO;
PSI_memory_key key_memory_ST_SCHEMA_TABLE;
PSI_memory_key key_memory_Slave_applier_json_diff_vector;
PSI_memory_key key_memory_Replica_job_group_group_relay_log_name;
PSI_memory_key key_memory_Sys_var_charptr_value;
PSI_memory_key key_memory_TABLE;
PSI_memory_key key_memory_TABLE_RULE_ENT;
PSI_memory_key key_memory_TABLE_sort_io_cache;
PSI_memory_key key_memory_TC_LOG_MMAP_pages;
PSI_memory_key key_memory_THD_Session_sysvar_resource_manager;
PSI_memory_key key_memory_THD_Session_tracker;
PSI_memory_key key_memory_THD_db;
PSI_memory_key key_memory_THD_handler_tables_hash;
PSI_memory_key key_memory_THD_variables;
PSI_memory_key key_memory_Unique_merge_buffer;
PSI_memory_key key_memory_Unique_sort_buffer;
PSI_memory_key key_memory_User_level_lock;
PSI_memory_key key_memory_xa_transaction_contexts;
PSI_memory_key key_memory_acl_mem;
PSI_memory_key key_memory_acl_memex;
PSI_memory_key key_memory_acl_cache;
PSI_memory_key key_memory_acl_map_cache;
PSI_memory_key key_memory_binlog_cache_mngr;
PSI_memory_key key_memory_binlog_pos;
PSI_memory_key key_memory_binlog_recover_exec;
PSI_memory_key key_memory_binlog_statement_buffer;
PSI_memory_key key_memory_bison_stack;
PSI_memory_key key_memory_blob_mem_storage;
PSI_memory_key key_memory_db_worker_hash_entry;
PSI_memory_key key_memory_delegate;
PSI_memory_key key_memory_errmsgs_server;
PSI_memory_key key_memory_global_system_variables;
PSI_memory_key key_memory_errmsgs_handler;
PSI_memory_key key_memory_handlerton_objects;
PSI_memory_key key_memory_hash_index_key_buffer;
PSI_memory_key key_memory_hash_join;
PSI_memory_key key_memory_help;
PSI_memory_key key_memory_histograms;
PSI_memory_key key_memory_host_cache_hostname;
PSI_memory_key key_memory_locked_table_list;
PSI_memory_key key_memory_locked_thread_list;
PSI_memory_key key_memory_my_str_malloc;
PSI_memory_key key_memory_opt_bin_logname;
PSI_memory_key key_memory_partition_syntax_buffer;
PSI_memory_key key_memory_persisted_variables_memroot;
PSI_memory_key key_memory_persisted_variables_unordered_map;
PSI_memory_key key_memory_persisted_variables_unordered_set;
PSI_memory_key key_memory_prepared_statement_infrastructure;
PSI_memory_key key_memory_prepared_statement_main_mem_root;
PSI_memory_key key_memory_partitions_prune_exec;
PSI_memory_key key_memory_queue_item;
PSI_memory_key key_memory_rm_db_mdl_reqs_root;
PSI_memory_key key_memory_rm_table_foreach_root;
PSI_memory_key key_memory_rpl_filter;
PSI_memory_key key_memory_rpl_replica_check_temp_dir;
PSI_memory_key key_memory_servers;
PSI_memory_key key_memory_shared_memory_name;
PSI_memory_key key_memory_show_replica_status_io_gtid_set;
PSI_memory_key key_memory_sp_head_call_root;
PSI_memory_key key_memory_sp_head_execute_root;
PSI_memory_key key_memory_sp_head_main_root;
PSI_memory_key key_memory_table_mapping_root;
PSI_memory_key key_memory_table_share;
PSI_memory_key key_memory_test_quick_select_exec;
PSI_memory_key key_memory_thd_main_mem_root;
PSI_memory_key key_memory_thd_timer;
PSI_memory_key key_memory_thd_transactions;
PSI_memory_key key_memory_user_conn;
PSI_memory_key key_memory_user_var_entry;
PSI_memory_key key_memory_user_var_entry_value;
PSI_memory_key key_memory_sp_cache;
PSI_memory_key key_memory_write_set_extraction;

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_info all_server_memory[] = {
    {&key_memory_locked_table_list, "Locked_tables_list::m_locked_tables_root",
     0, 0, "Memroot for list of locked tables."},
    {&key_memory_locked_thread_list, "display_table_locks", PSI_FLAG_THREAD, 0,
     "Debug utility."},
    {&key_memory_thd_transactions, "THD::transactions::mem_root",
     PSI_FLAG_THREAD, 0, "Transaction context information per session."},
    {&key_memory_delegate, "Delegate::memroot", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_acl_mem, "sql_acl_mem", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_acl_memex, "sql_acl_memex", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_acl_cache, "acl_cache", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_acl_map_cache, "acl_map_cache", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_thd_main_mem_root, "THD::main_mem_root",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0,
     "Main mem root used for e.g. the query arena."},
    {&key_memory_help, "help", 0, 0,
     "Temporary memroot used to print help texts as part of usage "
     "description."},
    {&key_memory_table_share, "TABLE_SHARE::mem_root",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Cache infrastructure and individual table shares."},
    {&key_memory_prepared_statement_infrastructure,
     "Prepared_statement::infrastructure",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0,
     "Map infrastructure for prepared statements per session."},
    {&key_memory_prepared_statement_main_mem_root,
     "Prepared_statement::main_mem_root",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0,
     "Mem root for each prepared statement for items etc."},
    {&key_memory_sp_cache, "THD::sp_cache", PSI_FLAG_MEM_COLLECT, 0,
     "Per session cache for stored programs."},
    {&key_memory_sp_head_main_root, "sp_head::main_mem_root", 0, 0,
     "Mem root for parsing and representation of stored programs."},
    {&key_memory_sp_head_execute_root, "sp_head::execute_mem_root",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0, "Mem root per instruction."},
    {&key_memory_sp_head_call_root, "sp_head::call_mem_root",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0,
     "Mem root for objects with same life time as stored program call."},
    {&key_memory_table_mapping_root, "table_mapping::m_mem_root", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_test_quick_select_exec, "test_quick_select",
     (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0, PSI_DOCUMENT_ME},
    {&key_memory_partitions_prune_exec, "Partition::prune_exec",
     PSI_FLAG_MEM_COLLECT, 0,
     "Mem root used temporarily while pruning partitions."},
    {&key_memory_binlog_recover_exec, "MYSQL_BIN_LOG::recover", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_blob_mem_storage, "Blob_mem_storage::storage",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},

    {&key_memory_String_value, "String::value", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Sys_var_charptr_value, "Sys_var_charptr::value", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_queue_item, "Queue::queue_item", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_THD_db, "THD::db", 0, 0, "Name of currently used schema."},
    {&key_memory_user_var_entry, "user_var_entry", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Replica_job_group_group_relay_log_name,
     "Replica_job_group::group_relay_log_name", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_binlog_cache_mngr, "binlog_cache_mngr", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Row_data_memory_memory, "Row_data_memory::memory", 0, 0,
     PSI_DOCUMENT_ME},

    {&key_memory_Gtid_set_to_string, "Gtid_set::to_string", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Gtid_state_to_string, "Gtid_state::to_string", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Owned_gtids_to_string, "Owned_gtids::to_string", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_log_event, "Log_event", 0, 0, PSI_DOCUMENT_ME},

    {&key_memory_Filesort_info_merge, "Filesort_info::merge",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},
    {&key_memory_Filesort_info_record_pointers,
     "Filesort_info::record_pointers", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Filesort_buffer_sort_keys, "Filesort_buffer::sort_keys",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},
    {&key_memory_errmsgs_handler, "errmsgs::handler", PSI_FLAG_ONLY_GLOBAL_STAT,
     0, "Handler error messages (HA_ERR_...)."},
    {&key_memory_handlerton_objects, "handlerton::objects",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, "Handlerton objects."},
    {&key_memory_xa_transaction_contexts, "XA::transaction_contexts", 0, 0,
     "Shared cache of XA transaction contexts."},
    {&key_memory_host_cache_hostname, "host_cache::hostname",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, "Hostname keys in the host_cache map."},
    {&key_memory_user_var_entry_value, "user_var_entry::value", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_User_level_lock, "User_level_lock", 0, 0,
     "Per session storage of user level locks."},
    {&key_memory_MYSQL_LOG_name, "MYSQL_LOG::name", PSI_FLAG_ONLY_GLOBAL_STAT,
     0, PSI_DOCUMENT_ME},
    {&key_memory_TC_LOG_MMAP_pages, "TC_LOG_MMAP::pages", 0, 0,
     "In-memory transaction coordinator log."},
    {&key_memory_IndexRangeScanIterator_mrr_buf_desc,
     "IndexRangeScanIterator::mrr_buf_desc", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Event_queue_element_for_exec_names,
     "Event_queue_element_for_exec::names", 0, 0,
     "Copy of schema- and event name in exec queue element."},
    {&key_memory_my_str_malloc, "my_str_malloc", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_MYSQL_BIN_LOG_basename, "MYSQL_BIN_LOG::basename",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_MYSQL_BIN_LOG_index, "MYSQL_BIN_LOG::index",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_MYSQL_RELAY_LOG_basename, "MYSQL_RELAY_LOG::basename",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_MYSQL_RELAY_LOG_index, "MYSQL_RELAY_LOG::index",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_rpl_filter, "rpl_filter memory", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_errmsgs_server, "errmsgs::server", PSI_FLAG_ONLY_GLOBAL_STAT,
     0, "In-memory representation of server error messages."},
    {&key_memory_Gis_read_stream_err_msg, "Gis_read_stream::err_msg", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Geometry_objects_data, "Geometry::ptr_and_wkb_data", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_MYSQL_LOCK, "MYSQL_LOCK", 0, 0, "Table locks per session."},
    {&key_memory_NET_buff, "NET::buff", 0, 0,
     "Buffer in the client protocol communications layer."},
    {&key_memory_NET_compress_packet, "NET::compress_packet", 0, 0,
     "Buffer used when compressing a packet."},
    {&key_memory_Event_scheduler_scheduler_param,
     "Event_scheduler::scheduler_param", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Infrastructure of the priority queue of events."},
    {&key_memory_Gtid_set_Interval_chunk, "Gtid_set::Interval_chunk", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Owned_gtids_sidno_to_hash, "Owned_gtids::sidno_to_hash", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_Sid_map_Node, "Sid_map::Node", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Gtid_state_group_commit_sidno,
     "Gtid_state::group_commit_sidno_locks", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Mutex_cond_array_Mutex_cond, "Mutex_cond_array::Mutex_cond", 0,
     0, PSI_DOCUMENT_ME},
    {&key_memory_TABLE_RULE_ENT, "TABLE_RULE_ENT", 0, 0, PSI_DOCUMENT_ME},

    {&key_memory_Rpl_info_table, "Rpl_info_table", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_Rpl_info_file_buffer, "Rpl_info_file::buffer", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_db_worker_hash_entry, "db_worker_hash_entry", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_rpl_replica_check_temp_dir, "rpl_replica::check_temp_dir", 0,
     0, PSI_DOCUMENT_ME},
    {&key_memory_REPLICA_INFO, "REPLICA_INFO", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_binlog_pos, "binlog_pos", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_HASH_ROW_ENTRY, "HASH_ROW_ENTRY", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_binlog_statement_buffer, "binlog_statement_buffer", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_partition_syntax_buffer, "Partition::syntax_buffer", 0, 0,
     "Buffer used for formatting the partition expression."},
    {&key_memory_READ_INFO, "READ_INFO", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_JOIN_CACHE, "JOIN_CACHE", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_TABLE_sort_io_cache, "TABLE::sort_io_cache",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},
    {&key_memory_DD_cache_infrastructure, "dd::infrastructure", 0, 0,
     "Infrastructure of the data dictionary structures."},
    {&key_memory_DD_column_statistics, "dd::column_statistics", 0, 0,
     "Column statistics histograms allocated."},
    {&key_memory_DD_default_values, "dd::default_values", 0, 0,
     "Temporary buffer for preparing column default values."},
    {&key_memory_DD_import, "dd::import", 0, 0,
     "File name handling while importing MyISAM tables."},
    {&key_memory_DD_objects, "dd::objects", 0, 0,
     "Memory occupied by the data dictionary objects."},
    {&key_memory_Unique_sort_buffer, "Unique::sort_buffer",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},
    {&key_memory_Unique_merge_buffer, "Unique::merge_buffer",
     PSI_FLAG_MEM_COLLECT, 0, PSI_DOCUMENT_ME},
    {&key_memory_TABLE, "TABLE", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Memory used by TABLE objects and their mem root."},
    {&key_memory_LOG_name, "LOG::file_name", 0, 0,
     "File name of slow log and general log."},
    {&key_memory_DD_String_type, "dd::String_type", 0, 0,
     "Character strings used by data dictionary objects."},
    {&key_memory_ST_SCHEMA_TABLE, "ST_SCHEMA_TABLE", 0, 0,
     "Structure describing an information schema table implemented by a "
     "plugin."},
    {&key_memory_PROFILE, "PROFILE", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_global_system_variables, "global_system_variables",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_THD_variables, "THD::variables", 0, 0,
     "Per session copy of global dynamic variables."},
    {&key_memory_shared_memory_name, "Shared_memory_name", 0, 0,
     "Communication through shared memory (windows)."},
    {&key_memory_bison_stack, "bison_stack", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_THD_handler_tables_hash, "THD::handler_tables_hash", 0, 0,
     "Hash map of tables used by HANDLER statements."},
    {&key_memory_hash_index_key_buffer, "hash_index_key_buffer", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_user_conn, "user_conn", 0, 0,
     "Objects describing user connections."},
    {&key_memory_LOG_POS_COORD, "LOG_POS_COORD", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_MPVIO_EXT_auth_info, "MPVIO_EXT::auth_info", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_opt_bin_logname, "opt_bin_logname", PSI_FLAG_ONLY_GLOBAL_STAT,
     0, PSI_DOCUMENT_ME},
    {&key_memory_READ_RECORD_cache, "READ_RECORD_cache", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_xa_recovered_transactions, "XA::recovered_transactions", 0, 0,
     "List infrastructure for recovered XA transactions."},
    {&key_memory_File_query_log_name, "File_query_log::name",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0, PSI_DOCUMENT_ME},
    {&key_memory_thd_timer, "thd_timer", 0, 0, "Thread timer object."},
    {&key_memory_THD_Session_tracker, "THD::Session_tracker", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_THD_Session_sysvar_resource_manager,
     "THD::Session_sysvar_resource_manager", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_show_replica_status_io_gtid_set,
     "show_replica_status_io_gtid_set", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_write_set_extraction, "write_set_extraction", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_JSON, "JSON", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_log_error_loaded_services, "log_error::loaded_services", 0, 0,
     "Memory allocated for duplicate log events."},
    {&key_memory_log_error_stack, "log_error::stack", PSI_FLAG_ONLY_GLOBAL_STAT,
     0, "Log events for the error log."},
    {&key_memory_log_sink_pfs, "log_sink_pfs", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_histograms, "histograms", 0, 0, PSI_DOCUMENT_ME},
    {&key_memory_hash_join, "hash_join", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_rm_table_foreach_root, "rm_table::foreach_root",
     PSI_FLAG_THREAD, 0,
     "Mem root for temporary objects allocated while dropping tables or the "
     "whole database."},
    {&key_memory_rm_db_mdl_reqs_root, "rm_db::mdl_reqs_root", PSI_FLAG_THREAD,
     0, "Mem root for allocating MDL requests while dropping datbase."},
    {&key_memory_persisted_variables_memroot, "Persisted_variables::memroot",
     PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Memory allocated to process persisted variables during server start-up "
     "and plugin/component initialization."},
    {&key_memory_persisted_variables_unordered_map,
     "Persisted_variables::unordered_map", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Memory allocated for in-memory maps for persisted variables"},
    {&key_memory_persisted_variables_unordered_set,
     "Persisted_variables::unordered_set", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     "Memory allocated for in-memory sets for persisted variables"}};

void register_server_memory_keys() {
  const char *category = "sql";
  int count = static_cast<int>(array_elements(all_server_memory));
  mysql_memory_register(category, all_server_memory, count);
}

#endif  // HAVE_PSI_INTERFACE
