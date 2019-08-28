/*
   Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "Ndbinfo.hpp"

#define JAM_FILE_ID 239



#define DECLARE_NDBINFO_TABLE(var, num)  \
static const struct  {                   \
  Ndbinfo::Table::Members m;             \
  Ndbinfo::Column col[num];              \
} ndbinfo_##var 


DECLARE_NDBINFO_TABLE(TABLES,3) =
{ { "tables", 3, 0, "metadata for tables available through ndbinfo" },
  {
    {"table_id",  Ndbinfo::Number, ""},

    {"table_name",Ndbinfo::String, ""},
    {"comment",   Ndbinfo::String, ""}
  }
};

DECLARE_NDBINFO_TABLE(COLUMNS,5) =
{ { "columns", 5, 0, "metadata for columns available through ndbinfo " },
  {
    {"table_id",    Ndbinfo::Number, ""},
    {"column_id",   Ndbinfo::Number, ""},

    {"column_name", Ndbinfo::String, ""},
    {"column_type", Ndbinfo::Number, ""},
    {"comment",     Ndbinfo::String, ""}
  }
};

DECLARE_NDBINFO_TABLE(TEST,5) =
{ { "test", 5, 0, "for testing" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"block_number",       Ndbinfo::Number, ""},
    {"block_instance",     Ndbinfo::Number, ""},

    {"counter",            Ndbinfo::Number, ""},
    {"counter2",           Ndbinfo::Number64, ""}
  }
};

DECLARE_NDBINFO_TABLE(POOLS,12) =
{ { "pools", 12, 0, "pool usage" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"block_number",       Ndbinfo::Number, ""},
    {"block_instance",     Ndbinfo::Number, ""},
    {"pool_name",          Ndbinfo::String, ""},

    {"used",               Ndbinfo::Number64, "currently in use"},
    {"total",              Ndbinfo::Number64, "total allocated"},
    {"high",               Ndbinfo::Number64, "in use high water mark"},
    {"entry_size",         Ndbinfo::Number64, "size in bytes of each object"},
    {"config_param1",      Ndbinfo::Number, "config param 1 affecting pool"},
    {"config_param2",      Ndbinfo::Number, "config param 2 affecting pool"},
    {"config_param3",      Ndbinfo::Number, "config param 3 affecting pool"},
    {"config_param4",      Ndbinfo::Number, "config param 4 affecting pool"}
  }
};

DECLARE_NDBINFO_TABLE(TRANSPORTERS, 11) =
{ { "transporters", 11, 0, "transporter status" },
  {
    {"node_id",              Ndbinfo::Number, "Node id reporting"},
    {"remote_node_id",       Ndbinfo::Number, "Node id at other end of link"},

    {"connection_status",    Ndbinfo::Number, "State of inter-node link"},
    
    {"remote_address",       Ndbinfo::String, "Address of remote node"},
    {"bytes_sent",           Ndbinfo::Number64, "Bytes sent to remote node"},
    {"bytes_received",       Ndbinfo::Number64, "Bytes received from remote node"},

    {"connect_count",        Ndbinfo::Number, "Number of times connected" },
    
    {"overloaded",           Ndbinfo::Number, "Is link reporting overload"},
    {"overload_count",       Ndbinfo::Number, "Number of overload onsets since connect"},
    
    {"slowdown",             Ndbinfo::Number, "Is link requesting slowdown"},
    {"slowdown_count",       Ndbinfo::Number, "Number of slowdown onsets since connect"}
  }
};

DECLARE_NDBINFO_TABLE(LOGSPACES, 7) =
{ { "logspaces", 7, 0, "logspace usage" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"log_type",           Ndbinfo::Number, "0 = REDO, 1 = DD-UNDO"},
    {"log_id",             Ndbinfo::Number, ""},
    {"log_part",           Ndbinfo::Number, ""},

    {"total",              Ndbinfo::Number64, "total allocated"},
    {"used",               Ndbinfo::Number64, "currently in use"},
    {"high",               Ndbinfo::Number64, "in use high water mark"}
  }
};

DECLARE_NDBINFO_TABLE(LOGBUFFERS, 7) =
{ { "logbuffers", 7, 0, "logbuffer usage" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"log_type",           Ndbinfo::Number, "0 = REDO, 1 = DD-UNDO, 2 = BACKUP-DATA, 3 = BACKUP-LOG"},
    {"log_id",             Ndbinfo::Number, ""},
    {"log_part",           Ndbinfo::Number, ""},

    {"total",              Ndbinfo::Number64, "total allocated"},
    {"used",               Ndbinfo::Number64, "currently in use"},
    {"high",               Ndbinfo::Number64, "in use high water mark"}
  }
};

DECLARE_NDBINFO_TABLE(RESOURCES,7) =
{ { "resources", 7, 0, "resources usage (a.k.a superpool)" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"resource_id",        Ndbinfo::Number, ""},

    {"reserved",           Ndbinfo::Number, "reserved for this resource"},
    {"used",               Ndbinfo::Number, "currently in use"},
    {"max",                Ndbinfo::Number, "max available"},
    {"high",               Ndbinfo::Number, "in use high water mark"},
    {"spare",              Ndbinfo::Number, "spare pages for restart"}
  }
};

DECLARE_NDBINFO_TABLE(COUNTERS,5) =
{ { "counters", 5, 0, "monotonic counters" },
  {
    {"node_id",            Ndbinfo::Number, ""},
    {"block_number",       Ndbinfo::Number, ""},
    {"block_instance",     Ndbinfo::Number, ""},
    {"counter_id",         Ndbinfo::Number, ""},

    {"val",                Ndbinfo::Number64, "monotonically increasing since process start"}
  }
};

DECLARE_NDBINFO_TABLE(NODES,5) =
{ { "nodes", 5, 0, "node status" },
  {
    {"node_id",            Ndbinfo::Number, ""},

    {"uptime",             Ndbinfo::Number64, "time in seconds that node has been running"},
    {"status",             Ndbinfo::Number, "starting/started/stopped etc."},
    {"start_phase",        Ndbinfo::Number, "start phase if node is starting"},
    {"config_generation",  Ndbinfo::Number, "configuration generation number"}
  }
};

DECLARE_NDBINFO_TABLE(DISKPAGEBUFFER, 9) =
{ { "diskpagebuffer", 9, 0, "disk page buffer info" },
  {
    {"node_id",                     Ndbinfo::Number, ""},
    {"block_instance",              Ndbinfo::Number, ""},

    {"pages_written",               Ndbinfo::Number64, "Pages written to disk"},
    {"pages_written_lcp",           Ndbinfo::Number64, "Pages written by local checkpoint"},
    {"pages_read",                  Ndbinfo::Number64, "Pages read from disk"},
    {"log_waits",                   Ndbinfo::Number64, "Page writes waiting for log to be written to disk"},
    {"page_requests_direct_return", Ndbinfo::Number64, "Page in buffer and no requests waiting for it"},
    {"page_requests_wait_queue",    Ndbinfo::Number64, "Page in buffer, but some requests are already waiting for it"},
    {"page_requests_wait_io",       Ndbinfo::Number64, "Page not in buffer, waiting to be read from disk"},
  }
};

DECLARE_NDBINFO_TABLE(THREADBLOCKS, 4) =
{ { "threadblocks", 4, 0, "which blocks are run in which threads" },
  {
    {"node_id",                     Ndbinfo::Number, "node id"},
    {"thr_no",                      Ndbinfo::Number, "thread number"},
    {"block_number",                Ndbinfo::Number, "block number"},
    {"block_instance",              Ndbinfo::Number, "block instance"},
  }
};

DECLARE_NDBINFO_TABLE(THREADSTAT, 18) =
{ { "threadstat", 18, 0, "Statistics on execution threads" },
  {
    //{"0123456701234567"}
    {"node_id",             Ndbinfo::Number, "node id"},
    {"thr_no",              Ndbinfo::Number, "thread number"},
    {"thr_nm",              Ndbinfo::String, "thread name"},
    {"c_loop",              Ndbinfo::Number64,"No of loops in main loop"},
    {"c_exec",              Ndbinfo::Number64,"No of signals executed"},
    {"c_wait",              Ndbinfo::Number64,"No of times waited for more input"},
    {"c_l_sent_prioa",      Ndbinfo::Number64,"No of prio A signals sent to own node"},
    {"c_l_sent_priob",      Ndbinfo::Number64,"No of prio B signals sent to own node"},
    {"c_r_sent_prioa",      Ndbinfo::Number64,"No of prio A signals sent to remote node"},
    {"c_r_sent_priob",      Ndbinfo::Number64,"No of prio B signals sent to remote node"},
    {"os_tid",              Ndbinfo::Number64,"OS thread id"},
    {"os_now",              Ndbinfo::Number64,"OS gettimeofday (millis)"},
    {"os_ru_utime",         Ndbinfo::Number64,"OS user CPU time (micros)"},
    {"os_ru_stime",         Ndbinfo::Number64,"OS system CPU time (micros)"},
    {"os_ru_minflt",        Ndbinfo::Number64,"OS page reclaims (soft page faults"},
    {"os_ru_majflt",        Ndbinfo::Number64,"OS page faults (hard page faults)"},
    {"os_ru_nvcsw",         Ndbinfo::Number64,"OS voluntary context switches"},
    {"os_ru_nivcsw",        Ndbinfo::Number64,"OS involuntary context switches"}
  }
};

DECLARE_NDBINFO_TABLE(TRANSACTIONS, 11) =
{ { "transactions", 11, 0, "transactions" },
  {
    {"node_id",             Ndbinfo::Number, "node id"},
    {"block_instance",      Ndbinfo::Number, "TC instance no"},
    {"objid",               Ndbinfo::Number, "Object id of transaction object"},
    {"apiref",              Ndbinfo::Number, "API reference"},
    {"transid0",            Ndbinfo::Number, "Transaction id"},
    {"transid1",            Ndbinfo::Number, "Transaction id"},
    {"state",               Ndbinfo::Number, "Transaction state"},
    {"flags",               Ndbinfo::Number, "Transaction flags"},
    {"c_ops",               Ndbinfo::Number, "No of operations in transaction" },
    {"outstanding",         Ndbinfo::Number, "Currently outstanding request" },
    {"timer",               Ndbinfo::Number, "Timer (seconds)"},
  }
};

DECLARE_NDBINFO_TABLE(OPERATIONS, 12) =
{ { "operations", 12, 0, "operations" },
  {
    {"node_id",             Ndbinfo::Number, "node id"},
    {"block_instance",      Ndbinfo::Number, "LQH instance no"},
    {"objid",               Ndbinfo::Number, "Object id of operation object"},
    {"tcref",               Ndbinfo::Number, "TC reference"},
    {"apiref",              Ndbinfo::Number, "API reference"},
    {"transid0",            Ndbinfo::Number, "Transaction id"},
    {"transid1",            Ndbinfo::Number, "Transaction id"},
    {"tableid",             Ndbinfo::Number, "Table id"},
    {"fragmentid",          Ndbinfo::Number, "Fragment id"},
    {"op",                  Ndbinfo::Number, "Operation type"},
    {"state",               Ndbinfo::Number, "Operation state"},
    {"flags",               Ndbinfo::Number, "Operation flags"}
  }
};

DECLARE_NDBINFO_TABLE(MEMBERSHIP, 13) =
{ { "membership", 13, 0, "membership" },
  {
    {"node_id",         Ndbinfo::Number, "node id"},
    {"group_id",        Ndbinfo::Number, "node group id"},
    {"left_node",       Ndbinfo::Number, "Left node in heart beat chain"},
    {"right_node",      Ndbinfo::Number, "Right node in heart beat chain"},
    {"president",       Ndbinfo::Number, "President nodeid"},
    {"successor",       Ndbinfo::Number, "President successor"},
    {"dynamic_id",      Ndbinfo::Number, "President, Configured_heartbeat order"},
    {"arbitrator",      Ndbinfo::Number, "Arbitrator nodeid"},
    {"arb_ticket",      Ndbinfo::String, "Arbitrator ticket"},
    {"arb_state",       Ndbinfo::Number, "Arbitrator state"},
    {"arb_connected",   Ndbinfo::Number, "Arbitrator connected"},
    {"conn_rank1_arbs", Ndbinfo::String, "Connected rank 1 arbitrators"},
    {"conn_rank2_arbs", Ndbinfo::String, "Connected rank 2 arbitrators"}
  }
};

DECLARE_NDBINFO_TABLE(DICT_OBJ_INFO, 7) =
{ { "dict_obj_info", 7, 0, "Dictionary object info" },
  {
    {"type",             Ndbinfo::Number,      "Type of dict object"},
    {"id",               Ndbinfo::Number,      "Object identity"},
    {"version",          Ndbinfo::Number,      "Object version"},
    {"state",            Ndbinfo::Number,      "Object state"},
    {"parent_obj_type",  Ndbinfo::Number,      "Parent object type" },
    {"parent_obj_id",    Ndbinfo::Number,      "Parent object id" },
    {"fq_name",          Ndbinfo::String,      "Fully qualified object name"}
  }
};

DECLARE_NDBINFO_TABLE(FRAG_MEM_USE, 15) =
{ { "frag_mem_use", 15, 0, "Per fragment space information" },
  {
    {"node_id",                  Ndbinfo::Number,    "node id"},
    {"block_instance",           Ndbinfo::Number,    "LDM instance number"},
    {"table_id",                 Ndbinfo::Number,    "Table identity"},
    {"fragment_num",             Ndbinfo::Number,    "Fragment number"},
    {"rows",                     Ndbinfo::Number64,  "Number of rows in table"},
    {"fixed_elem_alloc_bytes",   Ndbinfo::Number64,
     "Number of bytes allocated for fixed-sized elements"},
    {"fixed_elem_free_bytes",    Ndbinfo::Number64,
     "Free bytes in fixed-size element pages"},
    {"fixed_elem_count",         Ndbinfo::Number64,  
     "Number of fixed size elements in use"},
    {"fixed_elem_size_bytes",    Ndbinfo::Number,
     "Length of each fixed sized element in bytes"},
    {"var_elem_alloc_bytes",     Ndbinfo::Number64,
     "Number of bytes allocated for var-size elements"},
    {"var_elem_free_bytes",      Ndbinfo::Number64,
     "Free bytes in var-size element pages"},
    {"var_elem_count",           Ndbinfo::Number64,
     "Number of var size elements in use"},
    {"tuple_l2pmap_alloc_bytes", Ndbinfo::Number64, 
     "Bytes in logical to physical page map for tuple store"},
    {"hash_index_l2pmap_alloc_bytes",  Ndbinfo::Number64,  
     "Bytes in logical to physical page map for the hash index"},
    {"hash_index_alloc_bytes",   Ndbinfo::Number64,  "Bytes in linear hash map"}
  }
};

DECLARE_NDBINFO_TABLE(DISK_WRITE_SPEED_BASE, 7) =
{ { "disk_write_speed_base", 7, 0,
      "Actual speed of disk writes per LDM thread, base data" },
  {
    {"node_id",                     Ndbinfo::Number, "node id"},
    {"thr_no",                      Ndbinfo::Number, "LDM thread instance"},
    {"millis_ago",                  Ndbinfo::Number64,
        "Milliseconds ago since this period finished"},
    {"millis_passed",               Ndbinfo::Number64,
        "Milliseconds passed in the period reported"},
    {"backup_lcp_bytes_written",    Ndbinfo::Number64,
        "Bytes written by backup and LCP in the period"},
    {"redo_bytes_written",               Ndbinfo::Number64,
        "Bytes written to REDO log in the period"},
    {"target_disk_write_speed",     Ndbinfo::Number64,
        "Target disk write speed in bytes per second at the measurement point"},
  }
};

DECLARE_NDBINFO_TABLE(DISK_WRITE_SPEED_AGGREGATE, 16) =
{ { "disk_write_speed_aggregate", 16, 0,
      "Actual speed of disk writes per LDM thread, aggregate data" },
  {
    {"node_id",                     Ndbinfo::Number, "node id"},
    {"thr_no",                      Ndbinfo::Number, "LDM thread instance"},
    {"backup_lcp_speed_last_sec",   Ndbinfo::Number64,
               "Number of bytes written by backup and LCP last second"},
    {"redo_speed_last_sec",         Ndbinfo::Number64,
               "Number of bytes written to REDO log last second"},
    {"backup_lcp_speed_last_10sec", Ndbinfo::Number64,
               "Number of bytes written by backup and LCP per second last"
               " 10 seconds"},
    {"redo_speed_last_10sec",       Ndbinfo::Number64,
               "Number of bytes written to REDO log per second last"
               " 10 seconds"},
    {"std_dev_backup_lcp_speed_last_10sec", Ndbinfo::Number64,
         "Standard deviation of Number of bytes written by backup and LCP"
         " per second last 10 seconds"},
    {"std_dev_redo_speed_last_10sec", Ndbinfo::Number64,
         "Standard deviation of Number of bytes written to REDO log"
         " per second last 10 seconds"},
    {"backup_lcp_speed_last_60sec", Ndbinfo::Number64,
               "Number of bytes written by backup and LCP per second last"
               " 60 seconds"},
    {"redo_speed_last_60sec",       Ndbinfo::Number64,
               "Number of bytes written to REDO log per second last"
               " 60 seconds"},
    {"std_dev_backup_lcp_speed_last_60sec", Ndbinfo::Number64,
         "Standard deviation of Number of bytes written by backup and LCP"
         " per second last 60 seconds"},
    {"std_dev_redo_speed_last_60sec", Ndbinfo::Number64,
         "Standard deviation of Number of bytes written to REDO log"
         " per second last 60 seconds"},
    {"slowdowns_due_to_io_lag",     Ndbinfo::Number64,
         "Number of seconds that we slowed down disk writes due to REDO "
         "log IO lagging"},
    {"slowdowns_due_to_high_cpu",   Ndbinfo::Number64,
         "Number of seconds we slowed down disk writes due to high CPU usage "
         "of LDM thread"},
    {"disk_write_speed_set_to_min", Ndbinfo::Number64,
         "Number of seconds we set disk write speed to a minimum"},
    {"current_target_disk_write_speed", Ndbinfo::Number64,
         "Current target of disk write speed in bytes per second"},
  }
};

DECLARE_NDBINFO_TABLE(FRAG_OPERATIONS, 28) =
{ { "frag_operations", 28, 0, "Per fragment operational information" },
  {
    {"node_id",                 Ndbinfo::Number,    "node id"},
    {"block_instance",          Ndbinfo::Number,    "LQH instance no"},
    {"table_id",                Ndbinfo::Number,    "Table identity"},
    {"fragment_num",            Ndbinfo::Number,    "Fragment number"},
    {"tot_key_reads",           Ndbinfo::Number64,  
     "Total number of key reads received"},
    {"tot_key_inserts",         Ndbinfo::Number64,  
     "Total number of key inserts received"},
    {"tot_key_updates",         Ndbinfo::Number64,  
     "Total number of key updates received"},
    {"tot_key_writes",          Ndbinfo::Number64,  
     "Total number of key writes received"},
    {"tot_key_deletes",         Ndbinfo::Number64,
     "Total number of key deletes received"},
    {"tot_key_refs",            Ndbinfo::Number64,
     "Total number of key operations refused by LDM"},
    {"tot_key_attrinfo_bytes",  Ndbinfo::Number64,
     "Total attrinfo bytes received for key operations"},
    {"tot_key_keyinfo_bytes",   Ndbinfo::Number64,
     "Total keyinfo bytes received for key operations"},
    {"tot_key_prog_bytes",      Ndbinfo::Number64,
     "Total bytes of filter programs for key operations"},
    {"tot_key_inst_exec",       Ndbinfo::Number64,
     "Total number of interpreter instructions executed for key operations"},
    {"tot_key_bytes_returned",  Ndbinfo::Number64,
     "Total number of bytes returned to client for key operations"},
    {"tot_frag_scans",          Ndbinfo::Number64,
     "Total number of fragment scans received"},
    {"tot_scan_rows_examined",  Ndbinfo::Number64,
     "Total number of rows examined by scans"},
    {"tot_scan_rows_returned",  Ndbinfo::Number64,
     "Total number of rows returned to client by scan"},
    {"tot_scan_bytes_returned",  Ndbinfo::Number64,
     "Total number of bytes returned to client by scans"},
    {"tot_scan_prog_bytes",     Ndbinfo::Number64,
     "Total bytes of scan filter programs"},
    {"tot_scan_bound_bytes",     Ndbinfo::Number64,
     "Total bytes of scan bounds"},
    {"tot_scan_inst_exec",   Ndbinfo::Number64,
     "Total number of interpreter instructions executed for scans"},
    {"tot_qd_frag_scans",       Ndbinfo::Number64,
     "Total number of fragment scans queued before exec"},
    {"conc_frag_scans",         Ndbinfo::Number,
     "Number of frag scans currently running"},
    {"conc_qd_plain_frag_scans", Ndbinfo::Number,
     "Number of tux frag scans currently queued"},
    {"conc_qd_tup_frag_scans",  Ndbinfo::Number,
     "Number of tup frag scans currently queued"},
    {"conc_qd_acc_frag_scans",  Ndbinfo::Number,
     "Number of acc frag scans currently queued"},
    {"tot_commits",  Ndbinfo::Number64,
     "Total number of committed row changes"}
  }
};

DECLARE_NDBINFO_TABLE(RESTART_INFO, 22) =
{ { "restart_info", 22, 0,
       "Times of restart phases in seconds and current state" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node id" },
    {"node_restart_status",                                 Ndbinfo::String,
     "Current state of node recovery"},
    {"node_restart_status_int",                             Ndbinfo::Number,
     "Current state of node recovery as number"},
    {"secs_to_complete_node_failure",                       Ndbinfo::Number,
     "Seconds to complete node failure handling" },
    {"secs_to_allocate_node_id",                            Ndbinfo::Number,
     "Seconds from node failure completion to allocation of node id" },
    {"secs_to_include_in_heartbeat_protocol",               Ndbinfo::Number,
     "Seconds from allocation of node id to inclusion in HB protocol" },
    {"secs_until_wait_for_ndbcntr_master",                     Ndbinfo::Number,
     "Seconds from included in HB protocol until we wait for ndbcntr master"},
    {"secs_wait_for_ndbcntr_master",                        Ndbinfo::Number,
     "Seconds we waited for being accepted by NDBCNTR master to start" },
    {"secs_to_get_start_permitted",                         Ndbinfo::Number,
     "Seconds from permit by master until all nodes accepted our start" },
    {"secs_to_wait_for_lcp_for_copy_meta_data",             Ndbinfo::Number,
     "Seconds waiting for LCP completion before copying meta data" },
    {"secs_to_copy_meta_data",                              Ndbinfo::Number,
     "Seconds to copy meta data to starting node from master" },
    {"secs_to_include_node",                                Ndbinfo::Number,
     "Seconds to wait for GCP and inclusion of all nodes into protocols" },
    {"secs_starting_node_to_request_local_recovery",        Ndbinfo::Number,
     "Seconds for starting node to request local recovery" },
    {"secs_for_local_recovery",                             Ndbinfo::Number,
     "Seconds for local recovery in starting node" },
    {"secs_restore_fragments",                              Ndbinfo::Number,
     "Seconds to restore fragments from LCP files" },
    {"secs_undo_disk_data",                                 Ndbinfo::Number,
     "Seconds to execute UNDO log on disk data part of records" },
    {"secs_exec_redo_log",                                  Ndbinfo::Number,
     "Seconds to execute REDO log on all restored fragments" },
    {"secs_index_rebuild",                                  Ndbinfo::Number,
     "Seconds to rebuild indexes on restored fragments" },
    {"secs_to_synchronize_starting_node",                   Ndbinfo::Number,
     "Seconds to synchronize starting node from live nodes" },
    {"secs_wait_lcp_for_restart",                           Ndbinfo::Number,
  "Seconds to wait for LCP start and completion before restart is completed" },
    {"secs_wait_subscription_handover",                     Ndbinfo::Number,
     "Seconds waiting for handover of replication subscriptions" },
    {"total_restart_secs",                                  Ndbinfo::Number,
     "Total number of seconds from node failure until node is started again" },
  }
};

DECLARE_NDBINFO_TABLE(TC_TIME_TRACK_STATS, 15) =
{ { "tc_time_track_stats", 15, 0,
      "Time tracking of transaction, key operations and scan ops" },
  {
    {"node_id",                     Ndbinfo::Number, "node id"},
    {"block_number",                Ndbinfo::Number, "Block number"},
    {"block_instance",              Ndbinfo::Number, "Block instance"},
    {"comm_node_id",                Ndbinfo::Number, "node_id of API or DB"},
    {"upper_bound",                 Ndbinfo::Number64,
        "Upper bound in micros of interval"},
    {"scans",                       Ndbinfo::Number64,
        "scan histogram interval"},
    {"scan_errors",                 Ndbinfo::Number64,
        "scan error histogram interval"},
    {"scan_fragments",              Ndbinfo::Number64,
        "scan fragment histogram interval"},
    {"scan_fragment_errors",        Ndbinfo::Number64,
        "scan fragment error histogram interval"},
    {"transactions",                Ndbinfo::Number64,
        "transaction histogram interval"},
    {"transaction_errors",          Ndbinfo::Number64,
        "transaction error histogram interval"},
    {"read_key_ops",                Ndbinfo::Number64,
        "read key operation histogram interval"},
    {"write_key_ops",               Ndbinfo::Number64,
        "write key operation histogram interval"},
    {"index_key_ops",               Ndbinfo::Number64,
        "index key operation histogram interval"},
    {"key_op_errors",               Ndbinfo::Number64,
        "key operation error histogram interval"},
  }
};

DECLARE_NDBINFO_TABLE(CONFIG_VALUES,12) =
{ { "config_values", 3, 0, "Configuration parameter values" },
  {
    {"node_id",           Ndbinfo::Number, ""},
    {"config_param",      Ndbinfo::Number, "Parameter number"},
    {"config_value",      Ndbinfo::String, "Parameter value"},
  }
};

DECLARE_NDBINFO_TABLE(THREADS, 4) =
{ { "threads", 4, 0,
    "Base table for threads" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node_id" },
    {"thr_no",                                              Ndbinfo::Number,
     "thread number"},
    {"thread_name",                                         Ndbinfo::String,
     "thread_name"},
    {"thread_description",                                  Ndbinfo::String,
     "thread_description"},
  }
};

DECLARE_NDBINFO_TABLE(CPUSTAT_50MS, 11) =
{ { "cpustat_50ms", 11, 0,
    "Thread CPU stats at 50 milliseconds intervals" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node_id" },
    {"thr_no",                                              Ndbinfo::Number,
     "thread number"},
    {"OS_user_time",                                        Ndbinfo::Number,
     "User time in microseconds as reported by OS" },
    {"OS_system_time",                                      Ndbinfo::Number,
     "System time in microseconds as reported by OS" },
    {"OS_idle_time",                                        Ndbinfo::Number,
     "Idle time in microseconds as reported by OS" },
    {"exec_time",                                           Ndbinfo::Number,
     "Execution time in microseconds as calculated by thread" },
    {"sleep_time",                                          Ndbinfo::Number,
     "Sleep time in microseconds as calculated by thread" },
    {"spin_time",                                          Ndbinfo::Number,
     "Spin time in microseconds as calculated by thread" },
    {"send_time",                                           Ndbinfo::Number,
     "Send time in microseconds as calculated by thread" },
    {"buffer_full_time",                                    Ndbinfo::Number,
     "Time spent with buffer full in microseconds as calculated by thread" },
    {"elapsed_time",                                        Ndbinfo::Number,
     "Elapsed time in microseconds for measurement" },
  }
};

DECLARE_NDBINFO_TABLE(CPUSTAT_1SEC, 11) =
{ { "cpustat_1sec", 11, 0,
    "Thread CPU stats at 1 second intervals" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node_id" },
    {"thr_no",                                              Ndbinfo::Number,
     "thread number"},
    {"OS_user_time",                                        Ndbinfo::Number,
     "User time in microseconds as reported by OS" },
    {"OS_system_time",                                      Ndbinfo::Number,
     "System time in microseconds as reported by OS" },
    {"OS_idle_time",                                        Ndbinfo::Number,
     "Idle time in microseconds as reported by OS" },
    {"exec_time",                                           Ndbinfo::Number,
     "Execution time in microseconds as calculated by thread" },
    {"sleep_time",                                          Ndbinfo::Number,
     "Sleep time in microseconds as calculated by thread" },
    {"spin_time",                                          Ndbinfo::Number,
     "Spin time in microseconds as calculated by thread" },
    {"send_time",                                           Ndbinfo::Number,
     "Send time in microseconds as calculated by thread" },
    {"buffer_full_time",                                    Ndbinfo::Number,
     "Time spent with buffer full in microseconds as calculated by thread" },
    {"elapsed_time",                                        Ndbinfo::Number,
     "Elapsed time in microseconds for measurement" },
  }
};

DECLARE_NDBINFO_TABLE(CPUSTAT_20SEC, 11) =
{ { "cpustat_20sec", 11, 0,
    "Thread CPU stats at 20 seconds intervals" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node_id" },
    {"thr_no",                                              Ndbinfo::Number,
     "thread number"},
    {"OS_user_time",                                        Ndbinfo::Number,
     "User time in microseconds as reported by OS" },
    {"OS_system_time",                                      Ndbinfo::Number,
     "System time in microseconds as reported by OS" },
    {"OS_idle_time",                                        Ndbinfo::Number,
     "Idle time in microseconds as reported by OS" },
    {"exec_time",                                           Ndbinfo::Number,
     "Execution time in microseconds as calculated by thread" },
    {"sleep_time",                                          Ndbinfo::Number,
     "Sleep time in microseconds as calculated by thread" },
    {"spin_time",                                          Ndbinfo::Number,
     "Spin time in microseconds as calculated by thread" },
    {"send_time",                                           Ndbinfo::Number,
     "Send time in microseconds as calculated by thread" },
    {"buffer_full_time",                                    Ndbinfo::Number,
     "Time spent with buffer full in microseconds as calculated by thread" },
    {"elapsed_time",                                        Ndbinfo::Number,
     "Elapsed time in microseconds for measurement" },
  }
};

DECLARE_NDBINFO_TABLE(CPUSTAT, 11) =
{ { "cpustat", 11, 0,
    "Thread CPU stats for last second" },
  {
    {"node_id",                                             Ndbinfo::Number,
     "node_id" },
    {"thr_no",                                              Ndbinfo::Number,
     "thread number"},
    {"OS_user",                                             Ndbinfo::Number,
     "Percentage time spent in user mode as reported by OS" },
    {"OS_system",                                           Ndbinfo::Number,
     "Percentage time spent in system mode as reported by OS" },
    {"OS_idle",                                             Ndbinfo::Number,
     "Percentage time spent in idle mode as reported by OS" },
    {"thread_exec",                                         Ndbinfo::Number,
     "Percentage time spent executing as calculated by thread" },
    {"thread_sleeping",                                     Ndbinfo::Number,
     "Percentage time spent sleeping as calculated by thread" },
    {"thread_spinning",                                     Ndbinfo::Number,
     "Percentage time spent spinning as calculated by thread" },
    {"thread_send",                                         Ndbinfo::Number,
     "Percentage time spent sending as calculated by thread" },
    {"thread_buffer_full",                                  Ndbinfo::Number,
     "Percentage time spent in buffer full as calculated by thread" },
    {"elapsed_time",                                        Ndbinfo::Number,
     "Elapsed time in microseconds for measurement" },
  }
};

DECLARE_NDBINFO_TABLE(FRAG_LOCKS, 14) =
{ { "frag_locks", 14, 0,
    "Per fragment lock information" },
  {
    {"node_id",                 Ndbinfo::Number,
       "node id"},
    {"block_instance",          Ndbinfo::Number,    
       "LQH instance no"},
    {"table_id",                Ndbinfo::Number,    
       "Table identity"},
    {"fragment_num",            Ndbinfo::Number,    
       "Fragment number"},
    {"ex_req",                  Ndbinfo::Number64,  
       "Exclusive row lock request count"},
    {"ex_imm_ok",               Ndbinfo::Number64,
       "Exclusive row lock immediate grants"},
    {"ex_wait_ok",              Ndbinfo::Number64,
       "Exclusive row lock grants with wait"},
    {"ex_wait_fail",            Ndbinfo::Number64,
       "Exclusive row lock failed grants"},
    {"sh_req",                  Ndbinfo::Number64,  
       "Shared row lock request count"},
    {"sh_imm_ok",               Ndbinfo::Number64,
       "Shared row lock immediate grants"},
    {"sh_wait_ok",              Ndbinfo::Number64,
       "Shared row lock grants with wait"},
    {"sh_wait_fail",            Ndbinfo::Number64,
       "Shared row lock failed grants"},
    {"wait_ok_millis",          Ndbinfo::Number64,
       "Time spent waiting before successfully "
       "claiming a lock"},
    {"wait_fail_millis",        Ndbinfo::Number64,
       "Time spent waiting before failing to "
       "claim a lock"}
  }
};

DECLARE_NDBINFO_TABLE(ACC_OPERATIONS, 15) =
{ { "acc_operations", 15, 0, "ACC operation info" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"block_instance",              Ndbinfo::Number,   "Block instance"},
    {"tableid",                     Ndbinfo::Number,   "Table id"},
    {"fragmentid",                  Ndbinfo::Number,   "Fragment id"},
    {"rowid",                       Ndbinfo::Number64, "Row id in fragment"},
    {"transid0",                    Ndbinfo::Number,   "Transaction id"},
    {"transid1",                    Ndbinfo::Number,   "Transaction id"},
    {"acc_op_id",                   Ndbinfo::Number,   "Operation id"},
    {"op_flags",                    Ndbinfo::Number,   "Operation flags"},
    {"prev_serial_op_id",           Ndbinfo::Number,   "Prev serial op id"},
    {"next_serial_op_id",           Ndbinfo::Number,   "Next serial op id"},
    {"prev_parallel_op_id",         Ndbinfo::Number,   "Prev parallel op id"},
    {"next_parallel_op_id",         Ndbinfo::Number,   "Next parallel op id"},
    {"duration_millis",             Ndbinfo::Number,   "Duration of wait/hold"},
    {"user_ptr",                    Ndbinfo::Number,   "Lock requestor context"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_DIST_STATUS, 13) =
{ { "table_distribution_status", 13, 0, "Table status in distribution handler" },
  {
    {"node_id",               Ndbinfo::Number,   "Node id"},
    {"table_id",              Ndbinfo::Number,   "Table id"},
    {"tab_copy_status",       Ndbinfo::Number,   "Copy status of the table"},
    {"tab_update_status",     Ndbinfo::Number,   "Update status of the table"},
    {"tab_lcp_status",        Ndbinfo::Number,   "LCP status of the table"},
    {"tab_status",            Ndbinfo::Number,   "Create status of the table"},
    {"tab_storage",           Ndbinfo::Number,   "Storage type of table"},
    {"tab_type",              Ndbinfo::Number,   "Type of table"},
    {"tab_partitions",        Ndbinfo::Number,   "Number of partitions in table"},
    {"tab_fragments",         Ndbinfo::Number,   "Number of fragments in table"},
    {"current_scan_count",    Ndbinfo::Number,   "Current number of active scans"},
    {"scan_count_wait",       Ndbinfo::Number,   "Number of scans waiting for"},
    {"is_reorg_ongoing",      Ndbinfo::Number,   "Is a table reorg ongoing on table"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_FRAGMENTS, 15) =
{ { "table_fragments", 15, 0, "Partitions of the tables" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"table_id",                    Ndbinfo::Number,   "Table id"},
    {"partition_id",                Ndbinfo::Number,   "Partition id"},
    {"fragment_id",                 Ndbinfo::Number,   "Fragment id"},
    {"partition_order",             Ndbinfo::Number,   "Order of fragment in partition"},
    {"log_part_id",                 Ndbinfo::Number,   "Log part id of fragment"},
    {"no_of_replicas",              Ndbinfo::Number,   "Number of replicas"},
    {"current_primary",             Ndbinfo::Number,   "Current primary node id"},
    {"preferred_primary",           Ndbinfo::Number,   "Preferred primary node id"},
    {"current_first_backup",        Ndbinfo::Number,   "Current first backup node id"},
    {"current_second_backup",       Ndbinfo::Number,   "Current second backup node id"},
    {"current_third_backup",        Ndbinfo::Number,   "Current third backup node id"},
    {"num_alive_replicas",          Ndbinfo::Number,   "Current number of alive replicas"},
    {"num_dead_replicas",           Ndbinfo::Number,   "Current number of dead replicas"},
    {"num_lcp_replicas",            Ndbinfo::Number,   "Number of replicas remaining to be LCP:ed"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_REPLICAS, 16) =
{ { "table_replicas", 16, 0, "Fragment replicas of the tables" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"table_id",                    Ndbinfo::Number,   "Table id"},
    {"fragment_id",                 Ndbinfo::Number,   "Fragment id"},
    {"initial_gci",                 Ndbinfo::Number,   "Initial GCI for table"},
    {"replica_node_id",             Ndbinfo::Number,   "Node id where replica is stored"},
    {"is_lcp_ongoing",              Ndbinfo::Number,   "Is LCP ongoing on this fragment"},
    {"num_crashed_replicas",        Ndbinfo::Number,   "Number of crashed replica instances"},
    {"last_max_gci_started",        Ndbinfo::Number,   "Last LCP Max GCI started"},
    {"last_max_gci_completed",      Ndbinfo::Number,   "Last LCP Max GCI completed"},
    {"last_lcp_id",                 Ndbinfo::Number,   "Last LCP id"},
    {"prev_lcp_id",                 Ndbinfo::Number,   "Previous LCP id"},
    {"prev_max_gci_started",        Ndbinfo::Number,   "Previous LCP Max GCI started"},
    {"prev_max_gci_completed",      Ndbinfo::Number,   "Previous LCP Max GCI completed"},
    {"last_create_gci",             Ndbinfo::Number,   "Last Create GCI of last crashed replica instance"},
    {"last_replica_gci",            Ndbinfo::Number,   "Last GCI of last crashed replica instance"},
    {"is_replica_alive",            Ndbinfo::Number,   "Is replica alive or not"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_DIST_STATUS_ALL, 13) =
{ { "table_distribution_status_all", 13, 0, "Table status in distribution handler" },
  {
    {"node_id",               Ndbinfo::Number,   "Node id"},
    {"table_id",              Ndbinfo::Number,   "Table id"},
    {"tab_copy_status",       Ndbinfo::Number,   "Copy status of the table"},
    {"tab_update_status",     Ndbinfo::Number,   "Update status of the table"},
    {"tab_lcp_status",        Ndbinfo::Number,   "LCP status of the table"},
    {"tab_status",            Ndbinfo::Number,   "Create status of the table"},
    {"tab_storage",           Ndbinfo::Number,   "Storage type of table"},
    {"tab_type",              Ndbinfo::Number,   "Type of table"},
    {"tab_partitions",        Ndbinfo::Number,   "Number of partitions in table"},
    {"tab_fragments",         Ndbinfo::Number,   "Number of fragments in table"},
    {"current_scan_count",    Ndbinfo::Number,   "Current number of active scans"},
    {"scan_count_wait",       Ndbinfo::Number,   "Number of scans waiting for"},
    {"is_reorg_ongoing",      Ndbinfo::Number,   "Is a table reorg ongoing on table"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_FRAGMENTS_ALL, 15) =
{ { "table_fragments_all", 15, 0, "Partitions of the tables" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"table_id",                    Ndbinfo::Number,   "Table id"},
    {"partition_id",                Ndbinfo::Number,   "Partition id"},
    {"fragment_id",                 Ndbinfo::Number,   "Fragment id"},
    {"partition_order",             Ndbinfo::Number,   "Order of fragment in partition"},
    {"log_part_id",                 Ndbinfo::Number,   "Log part id of fragment"},
    {"no_of_replicas",              Ndbinfo::Number,   "Number of replicas"},
    {"current_primary",             Ndbinfo::Number,   "Current primary node id"},
    {"preferred_primary",           Ndbinfo::Number,   "Preferred primary node id"},
    {"current_first_backup",        Ndbinfo::Number,   "Current first backup node id"},
    {"current_second_backup",       Ndbinfo::Number,   "Current second backup node id"},
    {"current_third_backup",        Ndbinfo::Number,   "Current third backup node id"},
    {"num_alive_replicas",          Ndbinfo::Number,   "Current number of alive replicas"},
    {"num_dead_replicas",           Ndbinfo::Number,   "Current number of dead replicas"},
    {"num_lcp_replicas",            Ndbinfo::Number,   "Number of replicas remaining to be LCP:ed"}
  }
};

DECLARE_NDBINFO_TABLE(TABLE_REPLICAS_ALL, 16) =
{ { "table_replicas_all", 16, 0, "Fragment replicas of the tables" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"table_id",                    Ndbinfo::Number,   "Table id"},
    {"fragment_id",                 Ndbinfo::Number,   "Fragment id"},
    {"initial_gci",                 Ndbinfo::Number,   "Initial GCI for table"},
    {"replica_node_id",             Ndbinfo::Number,   "Node id where replica is stored"},
    {"is_lcp_ongoing",              Ndbinfo::Number,   "Is LCP ongoing on this fragment"},
    {"num_crashed_replicas",        Ndbinfo::Number,   "Number of crashed replica instances"},
    {"last_max_gci_started",        Ndbinfo::Number,   "Last LCP Max GCI started"},
    {"last_max_gci_completed",      Ndbinfo::Number,   "Last LCP Max GCI completed"},
    {"last_lcp_id",                 Ndbinfo::Number,   "Last LCP id"},
    {"prev_lcp_id",                 Ndbinfo::Number,   "Previous LCP id"},
    {"prev_max_gci_started",        Ndbinfo::Number,   "Previous LCP Max GCI started"},
    {"prev_max_gci_completed",      Ndbinfo::Number,   "Previous LCP Max GCI completed"},
    {"last_create_gci",             Ndbinfo::Number,   "Last Create GCI of last crashed replica instance"},
    {"last_replica_gci",            Ndbinfo::Number,   "Last GCI of last crashed replica instance"},
    {"is_replica_alive",            Ndbinfo::Number,   "Is replica alive or not"}
  }
};

DECLARE_NDBINFO_TABLE(STORED_TABLES, 20) =
{ { "stored_tables", 20, 0, "Information about stored tables" },
  {
    {"node_id",                     Ndbinfo::Number,   "node_id"},
    {"table_id",                    Ndbinfo::Number,   "Table id"},
    {"logged_table",                Ndbinfo::Number,   "Is table logged"},
    {"row_contains_gci",            Ndbinfo::Number,   "Does table rows contains GCI"},
    {"row_contains_checksum",       Ndbinfo::Number,   "Does table rows contain checksum"},
    {"temporary_table",             Ndbinfo::Number,   "Is table temporary"},
    {"force_var_part",              Ndbinfo::Number,   "Force var part active"},
    {"read_backup",                 Ndbinfo::Number,   "Is backup replicas read"},
    {"fully_replicated",            Ndbinfo::Number,   "Is table fully replicated"},
    {"extra_row_gci",               Ndbinfo::Number,   "extra_row_gci"},
    {"extra_row_author",            Ndbinfo::Number,   "extra_row_author"},
    {"storage_type",                Ndbinfo::Number,   "Storage type of table"},
    {"hashmap_id",                  Ndbinfo::Number,   "Hashmap id"},
    {"hashmap_version",             Ndbinfo::Number,   "Hashmap version"},
    {"table_version",               Ndbinfo::Number,   "Table version"},
    {"fragment_type",               Ndbinfo::Number,   "Type of fragmentation"},
    {"partition_balance",           Ndbinfo::Number,   "Partition balance"},
    {"create_gci",                  Ndbinfo::Number,   "GCI in which table was created"},
    {"backup_locked",               Ndbinfo::Number,   "Locked for backup"},
    {"single_user_mode",            Ndbinfo::Number,   "Is single user mode active"}
  }
};

DECLARE_NDBINFO_TABLE(PROCESSES, 8) =
{ { "processes", 8, 0, "Process ID and Name information for connected nodes" },
  {
    { "reporting_node_id",         Ndbinfo::Number,    "Reporting data node ID"},
    { "node_id",                   Ndbinfo::Number,    "Connected node ID"},
    { "node_type",                 Ndbinfo::Number,    "Type of node"},
    { "node_version",              Ndbinfo::String,    "Node MySQL Cluster version string"},
    { "process_id",                Ndbinfo::Number,    "PID of node process on host"},
    { "angel_process_id",          Ndbinfo::Number,    "PID of node\\\'s angel process"},
    { "process_name",              Ndbinfo::String,    "Node\\\'s executable process name"},
    { "service_URI",               Ndbinfo::String,    "URI for service provided by node"}
  }
};

DECLARE_NDBINFO_TABLE(CONFIG_NODES, 4) =
{ { "config_nodes", 4, 0, "All nodes of current cluster configuration" },
  {
    { "reporting_node_id",         Ndbinfo::Number,    "Reporting data node ID"},
    { "node_id",                   Ndbinfo::Number,    "Configured node ID"},
    { "node_type",                 Ndbinfo::Number,    "Configured node type"},
    { "node_hostname",             Ndbinfo::String,    "Configured hostname"}
  }
};

#define DBINFOTBL(x) { Ndbinfo::x##_TABLEID, (Ndbinfo::Table*)&ndbinfo_##x }

static
struct ndbinfo_table_list_entry {
  Ndbinfo::TableId id;
  Ndbinfo::Table * table;
} ndbinfo_tables[] = {
  // NOTE! the tables must be added to the list in the same order
  // as they are in "enum TableId"
  DBINFOTBL(TABLES),
  DBINFOTBL(COLUMNS),
  DBINFOTBL(TEST),
  DBINFOTBL(POOLS),
  DBINFOTBL(TRANSPORTERS),
  DBINFOTBL(LOGSPACES),
  DBINFOTBL(LOGBUFFERS),
  DBINFOTBL(RESOURCES),
  DBINFOTBL(COUNTERS),
  DBINFOTBL(NODES),
  DBINFOTBL(DISKPAGEBUFFER),
  DBINFOTBL(THREADBLOCKS),
  DBINFOTBL(THREADSTAT),
  DBINFOTBL(TRANSACTIONS),
  DBINFOTBL(OPERATIONS),
  DBINFOTBL(MEMBERSHIP),
  DBINFOTBL(DICT_OBJ_INFO),
  DBINFOTBL(FRAG_MEM_USE),
  DBINFOTBL(DISK_WRITE_SPEED_BASE),
  DBINFOTBL(DISK_WRITE_SPEED_AGGREGATE),
  DBINFOTBL(FRAG_OPERATIONS),
  DBINFOTBL(RESTART_INFO),
  DBINFOTBL(TC_TIME_TRACK_STATS),
  DBINFOTBL(CONFIG_VALUES),
  DBINFOTBL(THREADS),
  DBINFOTBL(CPUSTAT_50MS),
  DBINFOTBL(CPUSTAT_1SEC),
  DBINFOTBL(CPUSTAT_20SEC),
  DBINFOTBL(CPUSTAT),
  DBINFOTBL(FRAG_LOCKS),
  DBINFOTBL(ACC_OPERATIONS),
  DBINFOTBL(TABLE_DIST_STATUS),
  DBINFOTBL(TABLE_FRAGMENTS),
  DBINFOTBL(TABLE_REPLICAS),
  DBINFOTBL(TABLE_DIST_STATUS_ALL),
  DBINFOTBL(TABLE_FRAGMENTS_ALL),
  DBINFOTBL(TABLE_REPLICAS_ALL),
  DBINFOTBL(STORED_TABLES),
  DBINFOTBL(PROCESSES),
  DBINFOTBL(CONFIG_NODES)
};

static int no_ndbinfo_tables =
  sizeof(ndbinfo_tables) / sizeof(ndbinfo_tables[0]);


int Ndbinfo::getNumTables(){
  return no_ndbinfo_tables;
}

const Ndbinfo::Table& Ndbinfo::getTable(int i)
{
  assert(i >= 0 && i < no_ndbinfo_tables);
  ndbinfo_table_list_entry& entry = ndbinfo_tables[i];
  assert(entry.id == i);
  return *entry.table;
}

const Ndbinfo::Table& Ndbinfo::getTable(Uint32 i)
{
  return getTable((int)i);
}

/** 
 * #undef is needed since this file is included by NdbInfoTables.cpp
 */
#undef JAM_FILE_ID
