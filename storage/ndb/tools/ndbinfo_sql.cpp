/*
   Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ndb_opts.h>
#include <util/BaseString.hpp>
#include "../src/kernel/vm/NdbinfoTables.cpp"
#include "DictTabInfo.hpp"

static char* opt_ndbinfo_db = (char*)"ndbinfo";
static char* opt_table_prefix = (char*)"ndb$";

static
struct my_option
my_long_options[] =
{
  { "database", 'd',
    "Name of the database used by ndbinfo",
    (uchar**) &opt_ndbinfo_db, (uchar**) &opt_ndbinfo_db, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "prefix", 256,
    "Prefix to use for all virtual tables loaded from NDB",
    (uchar**) &opt_table_prefix, (uchar**) &opt_table_prefix, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

/*
 * Put views in alphabetical order.
 * No view should depend on another view.
 */
struct view {
  const char* name;
  const char* sql;
} views[] =
{
  { "arbitrator_validity_detail",
    "SELECT node_id, "
    "arbitrator, "
    "arb_ticket, "
    "CASE arb_connected"
    "  WHEN 1 THEN \"Yes\""
    "  ELSE \"No\""
    " END AS arb_connected, "
    "CASE arb_state"
    "  WHEN 0 THEN \"ARBIT_NULL\""
    "  WHEN 1 THEN \"ARBIT_INIT\""
    "  WHEN 2 THEN \"ARBIT_FIND\""
    "  WHEN 3 THEN \"ARBIT_PREP1\""
    "  WHEN 4 THEN \"ARBIT_PREP2\""
    "  WHEN 5 THEN \"ARBIT_START\""
    "  WHEN 6 THEN \"ARBIT_RUN\""
    "  WHEN 7 THEN \"ARBIT_CHOOSE\""
    "  WHEN 8 THEN \"ARBIT_CRASH\""
    "  ELSE \"UNKNOWN\""
    " END AS arb_state "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>membership` "
    "ORDER BY arbitrator, arb_connected DESC"
  },
  { "arbitrator_validity_summary",
    "SELECT arbitrator, "
    "arb_ticket, "
    "CASE arb_connected"
    "  WHEN 1 THEN \"Yes\""
    "  ELSE \"No\""
    " END AS arb_connected, "
    "count(*) as consensus_count "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>membership` "
    "GROUP BY arbitrator, arb_ticket, arb_connected"
  },
  // The blocks, dict_obj_types and config_params used
  // to be stored in a different engine but have now
  // been folded into hardcoded ndbinfo tables whose
  // name include the special prefix.
  // These views are defined to provide backward compatibility
  // for code using the old names.
  { "blocks",
    "SELECT block_number, block_name "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>blocks`"
  },
  {
    "cluster_locks",
    "SELECT "
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`node_id` AS `node_id`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`block_instance` AS `block_instance`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`tableid` AS `tableid`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`fragmentid` AS `fragmentid`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`rowid` AS `rowid`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`transid0` + "
    "(`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`transid1` << 32) AS `transid`,"
    /* op_flags meanings come from DbaccMain.cpp */
    /* 'S'hared or 'X'clusive */
    "(case (`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`op_flags` & 0x10) "
    "when 0 then \"S\" else \"X\" end) AS `mode`,"
    /* 'W'aiting or 'H'olding */
    "(case (`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`op_flags` & 0x80) "
    "when 0 then \"W\" else \"H\" end) AS `state`,"
    /* '*' indicates operation 'owning' the lock - an internal detail, can help
     * understanding
     */
    "(case (`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`op_flags` & 0x40) "
    "when 0 then \"\" else \"*\" end) as `detail`,"
    "case (`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`op_flags` & 0xf) "
    "when 0 then \"READ\" when 1 then \"UPDATE\" when 2 then \"INSERT\""
    "when 3 then \"DELETE\" when 5 then \"READ\" when 6 then \"REFRESH\""
    "when 7 then \"UNLOCK\" when 8 then \"SCAN\" ELSE\"<unknown>\" END as `op`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`duration_millis` as `duration_millis`,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`acc_op_id` AS `lock_num`,"
    "if(`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`op_flags` & 0xc0 = 0,"
    "`<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`.`prev_serial_op_id`"
    ", NULL) as `waiting_for` "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>acc_operations`"
  },
  { "cluster_operations",
    "SELECT"
    " o.node_id,"
    " o.block_instance,"
    " o.transid0 + (o.transid1 << 32) as transid,"
    " case o.op "
    " when 1 then \"READ\""
    " when 2 then \"READ-SH\""
    " when 3 then \"READ-EX\""
    " when 4 then \"INSERT\""
    " when 5 then \"UPDATE\""
    " when 6 then \"DELETE\""
    " when 7 then \"WRITE\""
    " when 8 then \"UNLOCK\""
    " when 9 then \"REFRESH\""
    " when 257 then \"SCAN\""
    " when 258 then \"SCAN-SH\""
    " when 259 then \"SCAN-EX\""
    " ELSE \"<unknown>\""
    " END as operation_type, "
    " s.state_friendly_name as state, "
    " o.tableid, "
    " o.fragmentid, "
    " (o.apiref & 65535) as client_node_id, "
    " (o.apiref >> 16) as client_block_ref, "
    " (o.tcref & 65535) as tc_node_id, "
    " ((o.tcref >> 16) & 511) as tc_block_no, "
    " ((o.tcref >> (16 + 9)) & 127) as tc_block_instance "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>operations` o"
    " LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dblqh_tcconnect_state` s"
    "        ON s.state_int_value = o.state"
  },
  { "cluster_transactions",
    "SELECT"
    " t.node_id,"
    " t.block_instance,"
    " t.transid0 + (t.transid1 << 32) as transid,"
    " s.state_friendly_name as state, "
    " t.c_ops as count_operations, "
    " t.outstanding as outstanding_operations, "
    " t.timer as inactive_seconds, "
    " (t.apiref & 65535) as client_node_id, "
    " (t.apiref >> 16) as client_block_ref "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>transactions` t"
    " LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dbtc_apiconnect_state` s"
    "        ON s.state_int_value = t.state"
  },
  { "config_nodes",
    "SELECT distinct node_id, "
    "CASE node_type"
    "  WHEN 0 THEN \"NDB\""
    "  WHEN 1 THEN \"API\""
    "  WHEN 2 THEN \"MGM\""
    "  ELSE NULL "
    " END AS node_type, "
    "node_hostname "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>config_nodes` "
    "ORDER BY node_id"
  },
  { "config_params",
    "SELECT param_number, param_name, param_description, param_type, param_default, "
    "param_min, param_max, param_mandatory, param_status "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>config_params`"
  },
  { "config_values",
    "SELECT node_id, config_param, config_value "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>config_values`"
  },
  { "counters",
    "SELECT node_id, b.block_name, block_instance, "
    "counter_id, "
    "CASE counter_id"
    "  WHEN 1 THEN \"ATTRINFO\""
    "  WHEN 2 THEN \"TRANSACTIONS\""
    "  WHEN 3 THEN \"COMMITS\""
    "  WHEN 4 THEN \"READS\""
    "  WHEN 5 THEN \"SIMPLE_READS\""
    "  WHEN 6 THEN \"WRITES\""
    "  WHEN 7 THEN \"ABORTS\""
    "  WHEN 8 THEN \"TABLE_SCANS\""
    "  WHEN 9 THEN \"RANGE_SCANS\""
    "  WHEN 10 THEN \"OPERATIONS\""
    "  WHEN 11 THEN \"READS_RECEIVED\""
    "  WHEN 12 THEN \"LOCAL_READS_SENT\""
    "  WHEN 13 THEN \"REMOTE_READS_SENT\""
    "  WHEN 14 THEN \"READS_NOT_FOUND\""
    "  WHEN 15 THEN \"TABLE_SCANS_RECEIVED\""
    "  WHEN 16 THEN \"LOCAL_TABLE_SCANS_SENT\""
    "  WHEN 17 THEN \"RANGE_SCANS_RECEIVED\""
    "  WHEN 18 THEN \"LOCAL_RANGE_SCANS_SENT\""
    "  WHEN 19 THEN \"REMOTE_RANGE_SCANS_SENT\""
    "  WHEN 20 THEN \"SCAN_BATCHES_RETURNED\""
    "  WHEN 21 THEN \"SCAN_ROWS_RETURNED\""
    "  WHEN 22 THEN \"PRUNED_RANGE_SCANS_RECEIVED\""
    "  WHEN 23 THEN \"CONST_PRUNED_RANGE_SCANS_RECEIVED\""
    "  WHEN 24 THEN \"LOCAL_READS\""
    "  WHEN 25 THEN \"LOCAL_WRITES\""
    "  WHEN 26 THEN \"LQHKEY_OVERLOAD\""
    "  WHEN 27 THEN \"LQHKEY_OVERLOAD_TC\""
    "  WHEN 28 THEN \"LQHKEY_OVERLOAD_READER\""
    "  WHEN 29 THEN \"LQHKEY_OVERLOAD_NODE_PEER\""
    "  WHEN 30 THEN \"LQHKEY_OVERLOAD_SUBSCRIBER\""
    "  WHEN 31 THEN \"LQHSCAN_SLOWDOWNS\""
    "  ELSE \"<unknown>\" "
    " END AS counter_name, "
    "val "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>counters` c "
    "LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>blocks` b "
    "ON c.block_number = b.block_number"
  },
  { "cpustat",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>cpustat`"
  },
  { "cpustat_1sec",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>cpustat_1sec`"
  },
  { "cpustat_20sec",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>cpustat_20sec`"
  },
  { "cpustat_50ms",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>cpustat_50ms`"
  },
  {"dict_obj_info",
   " SELECT * "
   "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info`"
  },
  { "dict_obj_types",
    "SELECT type_id, type_name "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_types`"
  },
  { "disk_write_speed_aggregate",
    "SELECT * FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>disk_write_speed_aggregate`"
  },
  { "disk_write_speed_aggregate_node",
    "SELECT"
    " node_id,"
    " SUM(backup_lcp_speed_last_sec) AS backup_lcp_speed_last_sec,"
    " SUM(redo_speed_last_sec) AS redo_speed_last_sec,"
    " SUM(backup_lcp_speed_last_10sec) AS backup_lcp_speed_last_10sec,"
    " SUM(redo_speed_last_10sec) AS redo_speed_last_10sec,"
    " SUM(backup_lcp_speed_last_60sec) AS backup_lcp_speed_last_60sec,"
    " SUM(redo_speed_last_60sec) AS redo_speed_last_60sec "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>disk_write_speed_aggregate` "
    "GROUP by node_id"
  },
  { "disk_write_speed_base",
    "SELECT * FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>disk_write_speed_base`"
  },
  { "diskpagebuffer",
     "SELECT node_id, block_instance, "
     "pages_written, pages_written_lcp, pages_read, log_waits, "
     "page_requests_direct_return, page_requests_wait_queue, page_requests_wait_io "
     "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>diskpagebuffer`"
  },
  { "error_messages",
    "SELECT error_code, error_description, error_status, error_classification "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>error_messages`"
  },
  {
    "locks_per_fragment",
    "SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, "
    "types.type_name AS type, table_id, node_id, block_instance, fragment_num, "
    "ex_req, ex_imm_ok, ex_wait_ok, ex_wait_fail, "
    "sh_req, sh_imm_ok, sh_wait_ok, sh_wait_fail, "
    "wait_ok_millis, wait_fail_millis "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>frag_locks` AS locks "
    "JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info` AS name "
    "ON name.id=locks.table_id AND name.type<=6 "
    "JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_types` AS types ON name.type=types.type_id "
    "LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info` AS parent_name "
    "ON name.parent_obj_id=parent_name.id AND "
    "name.parent_obj_type=parent_name.type"
  },
  { "logbuffers",
    "SELECT node_id, "
    " CASE log_type"
    "  WHEN 0 THEN \"REDO\""
    "  WHEN 1 THEN \"DD-UNDO\""
    "  WHEN 2 THEN \"BACKUP-DATA\""
    "  WHEN 3 THEN \"BACKUP-LOG\""
    "  ELSE \"<unknown>\" "
    " END AS log_type, "
    "log_id, log_part, total, used "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>logbuffers`"
  },
  { "logspaces",
    "SELECT node_id, "
    " CASE log_type"
    "  WHEN 0 THEN \"REDO\""
    "  WHEN 1 THEN \"DD-UNDO\""
    "  ELSE NULL "
    " END AS log_type, "
    "log_id, log_part, total, used "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>logspaces`"
  },
  { "membership",
    "SELECT node_id, group_id, left_node, right_node, president, successor, "
    "dynamic_id & 0xFFFF AS succession_order, "
    "dynamic_id >> 16 AS Conf_HB_order, "
    "arbitrator, arb_ticket, "
    "CASE arb_state"
    "  WHEN 0 THEN \"ARBIT_NULL\""
    "  WHEN 1 THEN \"ARBIT_INIT\""
    "  WHEN 2 THEN \"ARBIT_FIND\""
    "  WHEN 3 THEN \"ARBIT_PREP1\""
    "  WHEN 4 THEN \"ARBIT_PREP2\""
    "  WHEN 5 THEN \"ARBIT_START\""
    "  WHEN 6 THEN \"ARBIT_RUN\""
    "  WHEN 7 THEN \"ARBIT_CHOOSE\""
    "  WHEN 8 THEN \"ARBIT_CRASH\""
    "  ELSE \"UNKNOWN\""
    " END AS arb_state, "
    "CASE arb_connected"
    "  WHEN 1 THEN \"Yes\""
    "  ELSE \"No\""
    " END AS arb_connected, "
    "conn_rank1_arbs AS connected_rank1_arbs, "
    "conn_rank2_arbs AS connected_rank2_arbs "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>membership`"
  },
  {
    "memory_per_fragment",
    /*
     * The test for name.type<=6 is there to elimiate matching non-table
     * objects (triggers, files etc.), since the 'id' of these may collide
     * with table ids.
     */
    "SELECT name.fq_name, parent_name.fq_name AS parent_fq_name," 
    "types.type_name AS type, table_id, node_id, block_instance, "
    "fragment_num, fixed_elem_alloc_bytes, fixed_elem_free_bytes, "
    "fixed_elem_size_bytes, fixed_elem_count, "
    "FLOOR(fixed_elem_free_bytes/fixed_elem_size_bytes) AS "
    "fixed_elem_free_count, var_elem_alloc_bytes, var_elem_free_bytes, "
    "var_elem_count, hash_index_alloc_bytes "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>frag_mem_use` AS space "
    "JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info` "
    "AS name ON name.id=space.table_id AND name.type<=6 JOIN "
    " `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_types` AS types ON name.type=types.type_id "
    "LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info` AS parent_name "
    "ON name.parent_obj_id=parent_name.id AND "
    "name.parent_obj_type=parent_name.type"
  },
  { "memoryusage",
    "SELECT node_id,"
    "  pool_name AS memory_type,"
    "  SUM(used*entry_size) AS used,"
    "  SUM(used) AS used_pages,"
    "  SUM(total*entry_size) AS total,"
    "  SUM(total) AS total_pages "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>pools` "
    "WHERE block_number = 254 "
    "GROUP BY node_id, memory_type"
  },
  { "nodes",
    "SELECT node_id, "
    "uptime, "
    "CASE status"
    "  WHEN 0 THEN \"NOTHING\""
    "  WHEN 1 THEN \"CMVMI\""
    "  WHEN 2 THEN \"STARTING\""
    "  WHEN 3 THEN \"STARTED\""
    "  WHEN 4 THEN \"SINGLEUSER\""
    "  WHEN 5 THEN \"STOPPING_1\""
    "  WHEN 6 THEN \"STOPPING_2\""
    "  WHEN 7 THEN \"STOPPING_3\""
    "  WHEN 8 THEN \"STOPPING_4\""
    "  ELSE \"<unknown>\" "
    " END AS status, "
    "start_phase, "
    "config_generation "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>nodes`"
  },
  {
    "operations_per_fragment",
    /*
     * This is the end-user view of ndb$frag_operations. It adds some
     * dictionary information such as the table name and type, and the name
     * of the parent table, if there is any.
     *
     * The test for name.type<=6 is there to elimiate matching non-table
     * objects (triggers, files etc.), since the 'id' of these may collide
     * with table ids.
     */
    "SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, "
    "types.type_name AS type, table_id, node_id, block_instance, fragment_num, "
    "tot_key_reads, tot_key_inserts, tot_key_updates, tot_key_writes, "
    "tot_key_deletes, tot_key_refs, tot_key_attrinfo_bytes,"
    "tot_key_keyinfo_bytes, tot_key_prog_bytes, tot_key_inst_exec, "
    "tot_key_bytes_returned, tot_frag_scans, tot_scan_rows_examined, "
    "tot_scan_rows_returned, tot_scan_bytes_returned, tot_scan_prog_bytes, "
    "tot_scan_bound_bytes, tot_scan_inst_exec, tot_qd_frag_scans, "
    "conc_frag_scans,"
    "conc_qd_plain_frag_scans+conc_qd_tup_frag_scans+conc_qd_acc_frag_scans "
    "AS conc_qd_frag_scans, "
    "tot_commits "
    "FROM ndbinfo.ndb$frag_operations AS ops "
    "JOIN ndbinfo.ndb$dict_obj_info AS name "
    "ON name.id=ops.table_id AND name.type<=6 "
    "JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_types` AS types ON name.type=types.type_id "
    "LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>dict_obj_info` AS parent_name "
    "ON name.parent_obj_id=parent_name.id AND "
    "name.parent_obj_type=parent_name.type"
  },
#if 0
  { "pools",
    "SELECT node_id, b.block_name, block_instance, pool_name, "
    "used, total, high, entry_size, cp1.param_name AS param_name1, "
    "cp2.param_name AS param_name2, cp3.param_name AS param_name3, "
    "cp4.param_name AS param_name4 "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>pools` p "
    "LEFT JOIN `<NDBINFO_DB>`.blocks b ON p.block_number = b.block_number "
    "LEFT JOIN `<NDBINFO_DB>`.config_params cp1 ON p.config_param1 = cp1.param_number "
    "LEFT JOIN `<NDBINFO_DB>`.config_params cp2 ON p.config_param2 = cp2.param_number "
    "LEFT JOIN `<NDBINFO_DB>`.config_params cp3 ON p.config_param3 = cp3.param_number "
    "LEFT JOIN `<NDBINFO_DB>`.config_params cp4 ON p.config_param4 = cp4.param_number"
  },
#endif
  { "processes",
    "SELECT DISTINCT node_id, "
    "CASE node_type"
    "  WHEN 0 THEN \"NDB\""
    "  WHEN 1 THEN \"API\""
    "  WHEN 2 THEN \"MGM\""
    "  ELSE NULL "
    " END AS node_type, "
    " node_version, "
    " NULLIF(process_id, 0) AS process_id, "
    " NULLIF(angel_process_id, 0) AS angel_process_id, "
    " process_name, service_URI "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>processes` "
    "ORDER BY node_id"
  },
  { "resources",
    "SELECT node_id, "
    " CASE resource_id"
    "  WHEN 0 THEN \"RESERVED\""
    "  WHEN 1 THEN \"TRANSACTION_MEMORY\""
    "  WHEN 2 THEN \"DISK_RECORDS\""
    "  WHEN 3 THEN \"DATA_MEMORY\""
    "  WHEN 4 THEN \"JOBBUFFER\""
    "  WHEN 5 THEN \"FILE_BUFFERS\""
    "  WHEN 6 THEN \"TRANSPORTER_BUFFERS\""
    "  WHEN 7 THEN \"DISK_PAGE_BUFFER\""
    "  WHEN 8 THEN \"QUERY_MEMORY\""
    "  WHEN 9 THEN \"SCHEMA_TRANS_MEMORY\""
    "  ELSE \"<unknown>\" "
    " END AS resource_name, "
    "reserved, used, max, spare "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>resources`"
  },
  {"restart_info",
   "SELECT * "
   "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>restart_info`"
  },
  /* server_locks view, reflecting server_operations view */
  {"server_locks",
   "SELECT map.mysql_connection_id, l.* "
   "FROM `<NDBINFO_DB>`.cluster_locks l "
   "JOIN information_schema.ndb_transid_mysql_connection_map map"
   " ON (map.ndb_transid >> 32) = (l.transid >> 32)"
  },
  { "server_operations",
    "SELECT map.mysql_connection_id, o.* "
    "FROM `<NDBINFO_DB>`.cluster_operations o "
    "JOIN information_schema.ndb_transid_mysql_connection_map map"
    "  ON (map.ndb_transid >> 32) = (o.transid >> 32)"
  },
  { "server_transactions",
    "SELECT map.mysql_connection_id, t.*"
    "FROM information_schema.ndb_transid_mysql_connection_map map "
    "JOIN `<NDBINFO_DB>`.cluster_transactions t "
    "  ON (map.ndb_transid >> 32) = (t.transid >> 32)"
  },
  { "table_distribution_status",
    "SELECT node_id AS node_id, "
    "table_id AS table_id, "
    "CASE tab_copy_status"
    " WHEN 0 THEN \"IDLE\""
    " WHEN 1 THEN \"SR_PHASE1_READ_PAGES\""
    " WHEN 2 THEN \"SR_PHASE2_READ_TABLE\""
    " WHEN 3 THEN \"SR_PHASE3_COPY_TABLE\""
    " WHEN 4 THEN \"REMOVE_NODE\""
    " WHEN 5 THEN \"LCP_READ_TABLE\""
    " WHEN 6 THEN \"COPY_TAB_REQ\""
    " WHEN 7 THEN \"COPY_NODE_STATE\""
    " WHEN 8 THEN \"ADD_TABLE_MASTER\""
    " WHEN 9 THEN \"ADD_TABLE_SLAVE\""
    " WHEN 10 THEN \"INVALIDATE_NODE_LCP\""
    " WHEN 11 THEN \"ALTER_TABLE\""
    " WHEN 12 THEN \"COPY_TO_SAVE\""
    " WHEN 13 THEN \"GET_TABINFO\""
    "  ELSE \"Invalid value\""
    " END AS tab_copy_status, "
    "CASE tab_update_status"
    " WHEN 0 THEN \"IDLE\""
    " WHEN 1 THEN \"LOCAL_CHECKPOINT\""
    " WHEN 2 THEN \"LOCAL_CHECKPOINT_QUEUED\""
    " WHEN 3 THEN \"REMOVE_NODE\""
    " WHEN 4 THEN \"COPY_TAB_REQ\""
    " WHEN 5 THEN \"ADD_TABLE_MASTER\""
    " WHEN 6 THEN \"ADD_TABLE_SLAVE\""
    " WHEN 7 THEN \"INVALIDATE_NODE_LCP\""
    " WHEN 8 THEN \"CALLBACK\""
    "  ELSE \"Invalid value\""
    " END AS tab_update_status, "
    "CASE tab_lcp_status"
    " WHEN 1 THEN \"ACTIVE\""
    " WHEN 2 THEN \"wRITING_TO_FILE\""
    " WHEN 3 THEN \"COMPLETED\""
    "  ELSE \"Invalid value\""
    " END AS tab_lcp_status, "
    "CASE tab_status"
    " WHEN 0 THEN \"IDLE\""
    " WHEN 1 THEN \"ACTIVE\""
    " WHEN 2 THEN \"CREATING\""
    " WHEN 3 THEN \"DROPPING\""
    "  ELSE \"Invalid value\""
    " END AS tab_status, "
    "CASE tab_storage"
    " WHEN 0 THEN \"NOLOGGING\""
    " WHEN 1 THEN \"NORMAL\""
    " WHEN 2 THEN \"TEMPORARY\""
    "  ELSE \"Invalid value\""
    " END AS tab_storage, "
    "tab_partitions AS tab_partitions, "
    "tab_fragments AS tab_fragments, "
    "current_scan_count AS current_scan_count, "
    "scan_count_wait AS scan_count_wait, "
    "is_reorg_ongoing AS is_reorg_ongoing "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>table_distribution_status`"
  },
  { "table_fragments",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>table_fragments`"
  },
  { "table_info",
    " SELECT "
    " table_id AS table_id, "
    " logged_table AS logged_table, "
    " row_contains_gci AS row_contains_gci, "
    " row_contains_checksum AS row_contains_checksum, "
    " read_backup AS read_backup, "
    " fully_replicated AS fully_replicated, "
    " CASE storage_type"
    " WHEN 0 THEN \"MEMORY\""
    " WHEN 1 THEN \"DISK\""
    " WHEN 2 THEN \"MEMORY\""
    "  ELSE \"Invalid value\""
    " END AS storage_type,"
    " hashmap_id AS hashmap_id, "
    " CASE partition_balance"
    " WHEN 4294967295 THEN \"SPECIFIC\""
    " WHEN 4294967294 THEN \"FOR_RP_BY_LDM\""
    " WHEN 4294967293 THEN \"FOR_RA_BY_LDM\""
    " WHEN 4294967292 THEN \"FOR_RP_BY_NODE\""
    " WHEN 4294967291 THEN \"FOR_RA_BY_NODE\""
    " WHEN 4294967290 THEN \"FOR_RA_BY_LDM_X_2\""
    " WHEN 4294967289 THEN \"FOR_RA_BY_LDM_X_3\""
    " WHEN 4294967288 THEN \"FOR_RA_BY_LDM_X_4\""
    " ELSE \"Invalid value\""
    " END AS partition_balance,"
    " create_gci AS create_gci "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>stored_tables`"
  },
  { "table_replicas",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>table_replicas`"
  },
  { "tc_time_track_stats",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>tc_time_track_stats`"
  },
  { "threadblocks",
    "SELECT t.node_id, t.thr_no, b.block_name, t.block_instance "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>threadblocks` t "
    "LEFT JOIN `<NDBINFO_DB>`.`<TABLE_PREFIX>blocks` b "
    "ON t.block_number = b.block_number"
  },
  { "threads",
    "SELECT * "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>threads`"
  },
  { "threadstat",
    "SELECT * FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>threadstat`"
  },
  { "transporters",
    "SELECT node_id, remote_node_id, "
    " CASE connection_status"
    "  WHEN 0 THEN \"CONNECTED\""
    "  WHEN 1 THEN \"CONNECTING\""
    "  WHEN 2 THEN \"DISCONNECTED\""
    "  WHEN 3 THEN \"DISCONNECTING\""
    "  ELSE NULL "
    " END AS status, "
    " remote_address, bytes_sent, bytes_received, "
    " connect_count, "
    " overloaded, overload_count, slowdown, slowdown_count "
    "FROM `<NDBINFO_DB>`.`<TABLE_PREFIX>transporters`"
  },
};

size_t num_views = sizeof(views)/sizeof(views[0]);


// These tables are hardcoded(aka. virtual) in ha_ndbinfo
struct lookup {
  const char* name;
  const char* columns;
} lookups[] =
{
  { "<TABLE_PREFIX>blocks",
    "block_number INT UNSIGNED, "
    "block_name VARCHAR(512)",
  },
  { "<TABLE_PREFIX>config_params",
    "param_number INT UNSIGNED, "
    "param_name VARCHAR(512), "
    "param_description VARCHAR(512), "
    "param_type VARCHAR(512), "
    "param_default VARCHAR(512), "
    "param_min VARCHAR(512), "
    "param_max VARCHAR(512), "
    "param_mandatory INT UNSIGNED, "
    "param_status VARCHAR(512)"
  },
  {
    "<TABLE_PREFIX>dblqh_tcconnect_state",
    "state_int_value INT UNSIGNED, "
    "state_name VARCHAR(256), "
    "state_friendly_name VARCHAR(256), "
    "state_description VARCHAR(256)",
  },
  {
    "<TABLE_PREFIX>dbtc_apiconnect_state",
    "state_int_value INT UNSIGNED, "
    "state_name VARCHAR(256), "
    "state_friendly_name VARCHAR(256), "
    "state_description VARCHAR(256)",
  },
  { "<TABLE_PREFIX>dict_obj_types",
    "type_id INT UNSIGNED, "
    "type_name VARCHAR(512)",
  },
  { "<TABLE_PREFIX>error_messages",
    "error_code INT UNSIGNED, "
    "error_description VARCHAR(512), "
    "error_status VARCHAR(512), "
    "error_classification VARCHAR(512)",
  }
};

size_t num_lookups = sizeof(lookups)/sizeof(lookups[0]);


struct replace {
  const char* tag;
  const char* string;
} replaces[] =
{
  {"<TABLE_PREFIX>", opt_table_prefix},
  {"<NDBINFO_DB>", opt_ndbinfo_db},
};

size_t num_replaces = sizeof(replaces)/sizeof(replaces[0]);


BaseString replace_tags(const char* str)
{
  BaseString result(str);
  for (size_t i = 0; i < num_replaces; i++)
  {
    Vector<BaseString> parts;
    const char* p = result.c_str();
    const char* tag = replaces[i].tag;

    /* Split on <tag> */
    const char* first;
    while((first = strstr(p, tag)))
    {
      BaseString part;
      part.assign(p, first - p);
      parts.push_back(part);
      p = first + strlen(tag);
    }
    parts.push_back(p);

    /* Put back together */
    BaseString res;
    const char* separator = "";
    for (unsigned j = 0; j < parts.size(); j++)
    {
      res.appfmt("%s%s", separator, parts[j].c_str());
      separator = replaces[i].string;
    }

    /* Save result from this loop */
    result = res;
  }
  return result;
}

static void
print_conditional_sql(const BaseString& sql)
{
  printf("SET @str=IF(@have_ndbinfo,'%s','SET @dummy = 0');\n",
         sql.c_str());
  printf("PREPARE stmt FROM @str;\n");
  printf("EXECUTE stmt;\n");
  printf("DROP PREPARE stmt;\n\n");
}

static int compar_table_names(const void* px, const void* py)
{
  const Ndbinfo::Table* const* x = static_cast<const Ndbinfo::Table* const*>(px);
  const Ndbinfo::Table* const* y = static_cast<const Ndbinfo::Table* const*>(py);
  return strcmp((*x)->m.name, (*y)->m.name);
}

int main(int argc, char** argv){

  BaseString sql;
  if ((handle_options(&argc, &argv, my_long_options, NULL)))
    return 2;

  printf("# Generated by ndbinfo_sql # DO NOT EDIT! # Begin\n");
  printf("# TABLE definitions from src/kernel/vm/NdbinfoTables.cpp\n");
  printf("# VIEW definitions from tools/ndbinfo_sql.cpp\n");
  printf("#\n");
  printf("# SQL commands for creating the tables in MySQL Server which\n");
  printf("# are used by the NDBINFO storage engine to access system\n");
  printf("# information and statistics from MySQL Cluster\n");
  printf("#\n\n");

  printf("# Use latin1 when creating ndbinfo objects\n");
  printf("SET NAMES 'latin1' COLLATE 'latin1_swedish_ci';\n\n");

  printf("# Only create objects if NDBINFO is supported\n");
  printf("SELECT @have_ndbinfo:= COUNT(*) FROM "
                  "information_schema.engines WHERE engine='NDBINFO' "
                  "AND support IN ('YES', 'DEFAULT');\n\n");

  printf("# Only create objects if version >= 7.1\n");
  sql.assfmt("SELECT @have_ndbinfo:="
             " (@@ndbinfo_version >= (7 << 16) | (1 << 8)) || @ndbinfo_skip_version_check");
  print_conditional_sql(sql);

  sql.assfmt("CREATE DATABASE IF NOT EXISTS `%s`", opt_ndbinfo_db);
  print_conditional_sql(sql);

  printf("# Set NDBINFO in offline mode during (re)create of tables\n");
  printf("# and views to avoid errors caused by no such table or\n");
  printf("# different table definition in NDB\n");
  sql.assfmt("SET @@global.ndbinfo_offline=TRUE");
  print_conditional_sql(sql);

  {
    // Lookup tables which existed in other engine before
    // they were hardcoded into ha_ndbinfo. Drop to allow
    // the new ndbinfo tables(and in some cases views) to
    // be created
    const char* old_lookups[] =
    {
      "blocks",
      "config_params",
      "dict_obj_types",
      "ndb$dblqh_tcconnect_state",
      "ndb$dbtc_apiconnect_state"
    };
    printf("# Drop obsolete lookups in %s\n", opt_ndbinfo_db);
    for (size_t i = 0; i < sizeof(old_lookups)/sizeof(old_lookups[0]); i++)
    {
      sql.assfmt("DROP TABLE IF EXISTS `%s`.`%s`",
                 opt_ndbinfo_db, old_lookups[i]);
      print_conditional_sql(sql);
    }
  }

  printf("# Drop any old views in %s\n", opt_ndbinfo_db);
  for (size_t i = 0; i < num_views; i++)
  {
    sql.assfmt("DROP VIEW IF EXISTS `%s`.`%s`",
               opt_ndbinfo_db, views[i].name);
    print_conditional_sql(sql);
  }

  printf("# Drop any old lookup tables in %s\n", opt_ndbinfo_db);
  for (size_t i = 0; i < num_lookups; i++)
  {
    BaseString table_name = replace_tags(lookups[i].name);

    sql.assfmt("DROP TABLE IF EXISTS `%s`.`%s`",
               opt_ndbinfo_db, table_name.c_str());
    print_conditional_sql(sql);
  }

  printf("# Recreate lookup tables in %s\n", opt_ndbinfo_db);
  const Ndbinfo::Table** tables = new const Ndbinfo::Table*[Ndbinfo::getNumTables()];
  for (int i = 0; i < Ndbinfo::getNumTables(); i++)
  {
    tables[i] = &Ndbinfo::getTable(i);
  }
  qsort(tables, Ndbinfo::getNumTables(), sizeof(tables[0]), compar_table_names);
  for (int i = 0; i < Ndbinfo::getNumTables(); i++)
  {
    const Ndbinfo::Table& table = *tables[i];

    printf("# %s.%s%s\n",
            opt_ndbinfo_db, opt_table_prefix, table.m.name);

    /* Drop the table if it exists */
    sql.assfmt("DROP TABLE IF EXISTS `%s`.`%s%s`",
               opt_ndbinfo_db, opt_table_prefix, table.m.name);
    print_conditional_sql(sql);

    /* Create the table */
    sql.assfmt("CREATE TABLE `%s`.`%s%s` (",
               opt_ndbinfo_db, opt_table_prefix, table.m.name);

    const char* separator = "";
    for(int j = 0; j < table.m.ncols ; j++)
    {
      const Ndbinfo::Column& col = table.col[j];

      sql.appfmt("%s", separator);
      separator = ",";

      sql.appfmt("`%s` ", col.name);

      switch(col.coltype)
      {
      case Ndbinfo::Number:
        sql.appfmt("INT UNSIGNED");
        break;
      case Ndbinfo:: Number64:
        sql.appfmt("BIGINT UNSIGNED");
        break;
      case Ndbinfo::String:
        sql.appfmt("VARCHAR(512)");
        break;
      default:
        fprintf(stderr, "unknown coltype: %d\n", col.coltype);
        abort();
        break;
      }

      if (col.comment[0] != '\0')
        sql.appfmt(" COMMENT \"%s\"", col.comment);

    }

    sql.appfmt(") COMMENT=\"%s\" ENGINE=NDBINFO", table.m.comment);

    print_conditional_sql(sql);

  }

  delete [] tables;

  printf("# Recreate handler local lookup tables in %s\n", opt_ndbinfo_db);
  const char* prev_name = "";
  for (size_t i = 0; i < num_lookups; i++)
  {
    lookup l = lookups[i];
    if (strcmp(prev_name, l.name) >= 0)
    {
      fprintf(stderr, "lookup definitions reordered: %s >= %s\n", prev_name, l.name);
      exit(2);
    }
    prev_name = l.name;

    BaseString table_name = replace_tags(l.name);
    printf("# %s.%s\n", opt_ndbinfo_db, table_name.c_str());

    /* Drop the table if it exists */
    sql.assfmt("DROP TABLE IF EXISTS `%s`.`%s`",
               opt_ndbinfo_db, table_name.c_str());
    print_conditional_sql(sql);

    /* Create lookup table */
    sql.assfmt("CREATE TABLE `%s`.`%s` (%s) ENGINE=NDBINFO",
               opt_ndbinfo_db, table_name.c_str(), l.columns);
    print_conditional_sql(sql);
  }

  printf("# Recreate views in %s\n", opt_ndbinfo_db);
  prev_name = "";
  for (size_t i = 0; i < num_views; i++)
  {
    view v = views[i];

    if (strcmp(prev_name, v.name) >= 0)
    {
      fprintf(stderr, "view definitions reordered: %s >= %s\n", prev_name, v.name);
      exit(3);
    }
    prev_name = v.name;

    printf("# %s.%s\n", opt_ndbinfo_db, v.name);

    BaseString view_sql = replace_tags(v.sql);

    /* Create or replace the view */
    BaseString sql;
    sql.assfmt("CREATE OR REPLACE DEFINER=`root`@`localhost` "
               "SQL SECURITY INVOKER VIEW `%s`.`%s` AS %s",
               opt_ndbinfo_db, v.name, view_sql.c_str());
    print_conditional_sql(sql);
  }

  printf("# Finally turn off offline mode\n");
  sql.assfmt("SET @@global.ndbinfo_offline=FALSE");
  print_conditional_sql(sql);
  printf("# Generated by ndbinfo_sql # DO NOT EDIT! # End\n");

  return 0;
}

