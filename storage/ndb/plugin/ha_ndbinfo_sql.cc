/*
   Copyright (c) 2009, 2023, Oracle and/or its affiliates.

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

#include "mysql/service_thd_alloc.h"
#include "sql/current_thd.h"
#include "sql/plugin_table.h"
#include "sql/sql_list.h"

#include "debugger/Ndbinfo.hpp"
#include "util/BaseString.hpp"

static constexpr const char *opt_table_prefix{"ndb$"};

/*
 * Put views in alphabetical order by view_name.
 * No view should depend on another view.
 *
 * During bootstrap, the views will be created, along with their schemas,
 * by code in sql/dd/ndbinfo_schema/init.cc
 *
 * To delete a view, rename a view, or move a view from one schema to another:
 * create a record containing the old schema_name and the old view_name,
 * with sql == nullptr. This will enable the obsolete view to be dropped
 * at metadata creation time.
 *
 */
static struct view {
  const char *schema_name;
  const char *view_name;
  const char *sql;
} views[] = {
    {"ndbinfo", "arbitrator_validity_detail",
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
     "FROM `ndbinfo`.`ndb$membership` "
     "ORDER BY arbitrator, arb_connected DESC"},
    {"ndbinfo", "arbitrator_validity_summary",
     "SELECT arbitrator, "
     "arb_ticket, "
     "CASE arb_connected"
     "  WHEN 1 THEN \"Yes\""
     "  ELSE \"No\""
     " END AS arb_connected, "
     "count(*) as consensus_count "
     "FROM `ndbinfo`.`ndb$membership` "
     "GROUP BY arbitrator, arb_ticket, arb_connected"},
    {"ndbinfo", "backup_id", "SELECT id FROM `ndbinfo`.`ndb$backup_id`"},
    {"ndbinfo", "blocks",
     "SELECT block_number, block_name "
     "FROM `ndbinfo`.`ndb$blocks`"},
    {"ndbinfo", "cluster_locks",
     "SELECT "
     "`ndbinfo`.`ndb$acc_operations`.`node_id` AS `node_id`,"
     "`ndbinfo`.`ndb$acc_operations`.`block_instance` AS "
     "`block_instance`,"
     "`ndbinfo`.`ndb$acc_operations`.`tableid` AS `tableid`,"
     "`ndbinfo`.`ndb$acc_operations`.`fragmentid` AS "
     "`fragmentid`,"
     "`ndbinfo`.`ndb$acc_operations`.`rowid` AS `rowid`,"
     "`ndbinfo`.`ndb$acc_operations`.`transid0` + "
     "(`ndbinfo`.`ndb$acc_operations`.`transid1` << 32) AS "
     "`transid`,"
     /* op_flags meanings come from DbaccMain.cpp */
     /* 'S'hared or 'X'clusive */
     "(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x10) "
     "when 0 then \"S\" else \"X\" end) AS `mode`,"
     /* 'W'aiting or 'H'olding */
     "(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x80) "
     "when 0 then \"W\" else \"H\" end) AS `state`,"
     /* '*' indicates operation 'owning' the lock - an internal detail, can help
      * understanding
      */
     "(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x40) "
     "when 0 then \"\" else \"*\" end) as `detail`,"
     "case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0xf) "
     "when 0 then \"READ\" when 1 then \"UPDATE\" when 2 then \"INSERT\""
     "when 3 then \"DELETE\" when 5 then \"READ\" when 6 then \"REFRESH\""
     "when 7 then \"UNLOCK\" when 8 then \"SCAN\" ELSE\"<unknown>\" END as "
     "`op`,"
     "`ndbinfo`.`ndb$acc_operations`.`duration_millis` as "
     "`duration_millis`,"
     "`ndbinfo`.`ndb$acc_operations`.`acc_op_id` AS `lock_num`,"
     "if(`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0xc0 = 0,"
     "`ndbinfo`.`ndb$acc_operations`.`prev_serial_op_id`"
     ", NULL) as `waiting_for` "
     "FROM `ndbinfo`.`ndb$acc_operations`"},
    {"ndbinfo", "cluster_operations",
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
     "FROM `ndbinfo`.`ndb$operations` o"
     " LEFT JOIN `ndbinfo`.`ndb$dblqh_tcconnect_state` s"
     "        ON s.state_int_value = o.state"},
    {"ndbinfo", "cluster_transactions",
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
     "FROM `ndbinfo`.`ndb$transactions` t"
     " LEFT JOIN `ndbinfo`.`ndb$dbtc_apiconnect_state` s"
     "        ON s.state_int_value = t.state"},
    {"ndbinfo", "config_nodes",
     "SELECT distinct node_id, "
     "CASE node_type"
     "  WHEN 0 THEN \"NDB\""
     "  WHEN 1 THEN \"API\""
     "  WHEN 2 THEN \"MGM\""
     "  ELSE NULL "
     " END AS node_type, "
     "node_hostname "
     "FROM `ndbinfo`.`ndb$config_nodes` "
     "ORDER BY node_id"},
    {"ndbinfo", "config_params",
     "SELECT param_number, param_name, param_description, param_type, "
     "param_default, "
     "param_min, param_max, param_mandatory, param_status "
     "FROM `ndbinfo`.`ndb$config_params`"},
    {"ndbinfo", "config_values",
     "SELECT node_id, config_param, config_value "
     "FROM `ndbinfo`.`ndb$config_values`"},
    {"ndbinfo", "counters",
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
     "FROM `ndbinfo`.`ndb$counters` c "
     "LEFT JOIN `ndbinfo`.`ndb$blocks` b "
     "ON c.block_number = b.block_number"},
    {"ndbinfo", "cpudata",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpudata`"},
    {"ndbinfo", "cpudata_1sec",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpudata_1sec`"},
    {"ndbinfo", "cpudata_20sec",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpudata_20sec`"},
    {"ndbinfo", "cpudata_50ms",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpudata_50ms`"},
    {"ndbinfo", "cpuinfo",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpuinfo`"},
    {"ndbinfo", "cpustat",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpustat`"},
    {"ndbinfo", "cpustat_1sec",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpustat_1sec`"},
    {"ndbinfo", "cpustat_20sec",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpustat_20sec`"},
    {"ndbinfo", "cpustat_50ms",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$cpustat_50ms`"},
    {"ndbinfo", "dict_obj_info",
     " SELECT * "
     "FROM `ndbinfo`.`ndb$dict_obj_info`"},
    {"ndbinfo", "dict_obj_tree",
     "WITH RECURSIVE tree (type, id, name,"
     "  parent_type, parent_id, parent_name,"
     "  root_type, root_id, root_name,"
     "  level,path, indented_name) AS ("
     "SELECT"
     "  type, id, CAST(fq_name AS CHAR), "  // Current info
     "  parent_obj_type, parent_obj_id, CAST(fq_name AS CHAR), "  // Parent info
     "  type, id, CAST(fq_name AS CHAR), "                        // Root info
     "  1, "                     // Current level
     "  CAST(fq_name AS CHAR),"  // Current path
     "  CAST(fq_name AS CHAR)"   // Current indented name
     "  FROM ndbinfo.dict_obj_info"
     "  WHERE parent_obj_id = 0 AND parent_obj_type = 0 "  // Only top level
     "UNION ALL "
     "SELECT"
     "  i.type, i.id, i.fq_name, "                        // Current info
     "  i.parent_obj_type, i.parent_obj_id, t.name, "     // Parent info
     "  t.root_type, t.root_id, t.root_name, "            // Root info
     "  t.level + 1, "                                    // Current level
     "  CONCAT(t.path, ' -> ', i.fq_name), "              // Current path
     "  CONCAT(REPEAT('  ', level),  '-> ', i.fq_name) "  // Current indented
                                                          // name
     "FROM tree t JOIN ndbinfo.dict_obj_info i "
     "ON t.type = i.parent_obj_type AND t.id = i.parent_obj_id"
     ") SELECT * FROM tree ORDER BY path"},
    {"ndbinfo", "dict_obj_types",
     "SELECT type_id, type_name "
     "FROM `ndbinfo`.`ndb$dict_obj_types`"},
    {"ndbinfo", "disk_write_speed_aggregate",
     "SELECT * FROM `ndbinfo`.`ndb$disk_write_speed_aggregate`"},
    {"ndbinfo", "disk_write_speed_aggregate_node",
     "SELECT"
     " node_id,"
     " SUM(backup_lcp_speed_last_sec) AS backup_lcp_speed_last_sec,"
     " SUM(redo_speed_last_sec) AS redo_speed_last_sec,"
     " SUM(backup_lcp_speed_last_10sec) AS backup_lcp_speed_last_10sec,"
     " SUM(redo_speed_last_10sec) AS redo_speed_last_10sec,"
     " SUM(backup_lcp_speed_last_60sec) AS backup_lcp_speed_last_60sec,"
     " SUM(redo_speed_last_60sec) AS redo_speed_last_60sec "
     "FROM `ndbinfo`.`ndb$disk_write_speed_aggregate` "
     "GROUP by node_id"},
    {"ndbinfo", "disk_write_speed_base",
     "SELECT * FROM `ndbinfo`.`ndb$disk_write_speed_base`"},
    {"ndbinfo", "diskpagebuffer",
     "SELECT node_id, block_instance, "
     "pages_written, pages_written_lcp, pages_read, log_waits, "
     "page_requests_direct_return, page_requests_wait_queue, "
     "page_requests_wait_io "
     "FROM `ndbinfo`.`ndb$diskpagebuffer`"},
    {"ndbinfo", "diskstat",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$diskstat`"},
    {"ndbinfo", "diskstats_1sec",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$diskstats_1sec`"},
    {"ndbinfo", "error_messages",
     "SELECT error_code, error_description, error_status, error_classification "
     "FROM `ndbinfo`.`ndb$error_messages`"},
    {"ndbinfo", "files",
     "SELECT id, type_name AS type, fq_name AS name, "
     "parent_obj_id as parent, tablespace_name as parent_name, "
     "free_extents, total_extents, extent_size, initial_size, "
     "maximum_size, autoextend_size "
     "FROM ndbinfo.dict_obj_info info "
     "JOIN ndbinfo.dict_obj_types types ON info.type = types.type_id "
     "LEFT OUTER JOIN information_schema.files f ON f.file_id = info.id "
     "AND f.engine = 'ndbcluster' "
     "WHERE info.type in (20,21) OR info.parent_obj_type in (20,21) "
     "ORDER BY parent, id"},
    {"ndbinfo", "hash_maps",
     "SELECT id, version, state, fq_name "
     "FROM ndbinfo.dict_obj_info WHERE type=24"},
    {"ndbinfo", "hwinfo",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$hwinfo`"},
    {"ndbinfo", "index_stats",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$index_stats`"},
    {"ndbinfo", "locks_per_fragment",
     "SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, "
     "types.type_name AS type, table_id, node_id, block_instance, "
     "fragment_num, "
     "ex_req, ex_imm_ok, ex_wait_ok, ex_wait_fail, "
     "sh_req, sh_imm_ok, sh_wait_ok, sh_wait_fail, "
     "wait_ok_millis, wait_fail_millis "
     "FROM `ndbinfo`.`ndb$frag_locks` AS locks "
     "JOIN `ndbinfo`.`ndb$dict_obj_info` AS name "
     "ON name.id=locks.table_id AND name.type<=6 "
     "JOIN `ndbinfo`.`ndb$dict_obj_types` AS types ON "
     "name.type=types.type_id "
     "LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name "
     "ON name.parent_obj_id=parent_name.id AND "
     "name.parent_obj_type=parent_name.type"},
    {"ndbinfo", "logbuffers",
     "SELECT node_id, "
     " CASE log_type"
     "  WHEN 0 THEN \"REDO\""
     "  WHEN 1 THEN \"DD-UNDO\""
     "  WHEN 2 THEN \"BACKUP-DATA\""
     "  WHEN 3 THEN \"BACKUP-LOG\""
     "  ELSE \"<unknown>\" "
     " END AS log_type, "
     "log_id, log_part, total, used "
     "FROM `ndbinfo`.`ndb$logbuffers`"},
    {"ndbinfo", "logspaces",
     "SELECT node_id, "
     " CASE log_type"
     "  WHEN 0 THEN \"REDO\""
     "  WHEN 1 THEN \"DD-UNDO\""
     "  ELSE NULL "
     " END AS log_type, "
     "log_id, log_part, total, used "
     "FROM `ndbinfo`.`ndb$logspaces`"},
    {"ndbinfo", "membership",
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
     "FROM `ndbinfo`.`ndb$membership`"},
    {"ndbinfo", "memory_per_fragment",
     /*
      * The test for name.type<=6 is there to eliminate matching non-table
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
     "FROM `ndbinfo`.`ndb$frag_mem_use` AS space "
     "JOIN `ndbinfo`.`ndb$dict_obj_info` "
     "AS name ON name.id=space.table_id AND name.type<=6 JOIN "
     " `ndbinfo`.`ndb$dict_obj_types` AS types ON "
     "name.type=types.type_id "
     "LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name "
     "ON name.parent_obj_id=parent_name.id AND "
     "name.parent_obj_type=parent_name.type"},
    {"ndbinfo", "memoryusage",
     "SELECT node_id,"
     "  pool_name AS memory_type,"
     "  SUM(used*entry_size) AS used,"
     "  SUM(used) AS used_pages,"
     "  SUM(total*entry_size) AS total,"
     "  SUM(total) AS total_pages "
     "FROM `ndbinfo`.`ndb$pools` "
     "WHERE block_number = 254 "
     "GROUP BY node_id, memory_type"},
    {"ndbinfo", "nodes",
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
     "FROM `ndbinfo`.`ndb$nodes`"},
    {"ndbinfo", "operations_per_fragment",
     /*
      * This is the end-user view of ndb$frag_operations. It adds some
      * dictionary information such as the table name and type, and the name
      * of the parent table, if there is any.
      *
      * The test for name.type<=6 is there to eliminate matching non-table
      * objects (triggers, files etc.), since the 'id' of these may collide
      * with table ids.
      */
     "SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, "
     "types.type_name AS type, table_id, node_id, block_instance, "
     "fragment_num, "
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
     "JOIN `ndbinfo`.`ndb$dict_obj_types` AS types ON "
     "name.type=types.type_id "
     "LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name "
     "ON name.parent_obj_id=parent_name.id AND "
     "name.parent_obj_type=parent_name.type"},
    {"ndbinfo", "pgman_time_track_stats",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$pgman_time_track_stats`"},
#if 0
  { "ndbinfo", "pools",
    "SELECT node_id, b.block_name, block_instance, pool_name, "
    "used, total, high, entry_size, cp1.param_name AS param_name1, "
    "cp2.param_name AS param_name2, cp3.param_name AS param_name3, "
    "cp4.param_name AS param_name4 "
    "FROM `ndbinfo`.`ndb$pools` p "
    "LEFT JOIN `ndbinfo`.blocks b ON p.block_number = b.block_number "
    "LEFT JOIN `ndbinfo`.config_params cp1 ON p.config_param1 = cp1.param_number "
    "LEFT JOIN `ndbinfo`.config_params cp2 ON p.config_param2 = cp2.param_number "
    "LEFT JOIN `ndbinfo`.config_params cp3 ON p.config_param3 = cp3.param_number "
    "LEFT JOIN `ndbinfo`.config_params cp4 ON p.config_param4 = cp4.param_number"
  },
#endif
    {"ndbinfo", "processes",
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
     "FROM `ndbinfo`.`ndb$processes` "
     "ORDER BY node_id"},
    {"ndbinfo", "resources",
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
     "FROM `ndbinfo`.`ndb$resources`"},
    {"ndbinfo", "restart_info",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$restart_info`"},
    /* server_locks view, reflecting server_operations view */
    {"ndbinfo", "server_locks",
     "SELECT map.mysql_connection_id, l.* "
     "FROM `ndbinfo`.cluster_locks l "
     "JOIN information_schema.ndb_transid_mysql_connection_map map"
     " ON (map.ndb_transid >> 32) = (l.transid >> 32)"},
    {"ndbinfo", "server_operations",
     "SELECT map.mysql_connection_id, o.* "
     "FROM `ndbinfo`.cluster_operations o "
     "JOIN information_schema.ndb_transid_mysql_connection_map map"
     "  ON (map.ndb_transid >> 32) = (o.transid >> 32)"},
    {"ndbinfo", "server_transactions",
     "SELECT map.mysql_connection_id, t.*"
     "FROM information_schema.ndb_transid_mysql_connection_map map "
     "JOIN `ndbinfo`.cluster_transactions t "
     "  ON (map.ndb_transid >> 32) = (t.transid >> 32)"},
    {"ndbinfo", "table_distribution_status",
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
     " WHEN 8 THEN \"ADD_TABLE_COORDINATOR\""
     " WHEN 9 THEN \"ADD_TABLE_PARTICIPANT\""
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
     " WHEN 5 THEN \"ADD_TABLE_COORDINATOR\""
     " WHEN 6 THEN \"ADD_TABLE_PARTICIPANT\""
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
     "FROM `ndbinfo`.`ndb$table_distribution_status`"},
    {"ndbinfo", "table_fragments",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$table_fragments`"},
    {"ndbinfo", "table_info",
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
     "FROM `ndbinfo`.`ndb$stored_tables`"},
    {"ndbinfo", "table_replicas",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$table_replicas`"},
    {"ndbinfo", "tc_time_track_stats",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$tc_time_track_stats`"},
    {"ndbinfo", "threadblocks",
     "SELECT t.node_id, t.thr_no, b.block_name, t.block_instance "
     "FROM `ndbinfo`.`ndb$threadblocks` t "
     "LEFT JOIN `ndbinfo`.`ndb$blocks` b "
     "ON t.block_number = b.block_number"},
    {"ndbinfo", "threads",
     "SELECT * "
     "FROM `ndbinfo`.`ndb$threads`"},
    {"ndbinfo", "threadstat", "SELECT * FROM `ndbinfo`.`ndb$threadstat`"},
    {"ndbinfo", "transporters",
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
     "FROM `ndbinfo`.`ndb$transporters`"},
};

static constexpr size_t num_views = sizeof(views) / sizeof(views[0]);

// These tables are hardcoded(aka. virtual) in ha_ndbinfo
static struct lookup {
  const char *schema_name;
  const char *lookup_table_name;
  const char *columns;
} lookups[] = {
    {"ndbinfo", "blobs",
     "table_id INT UNSIGNED NOT NULL, "
     "database_name varchar(64) NOT NULL, "
     "table_name varchar(64) NOT NULL, "
     "column_id INT UNSIGNED NOT NULL, "
     "column_name varchar(64) NOT NULL, "
     "inline_size int unsigned NOT NULL, "
     "part_size int unsigned NOT NULL, "
     "stripe_size int unsigned NOT NULL, "
     "blob_table_name varchar(128) not null"},
    {"ndbinfo", "dictionary_columns",
     "table_id INT UNSIGNED NOT NULL, "
     "column_id INT UNSIGNED NOT NULL, "
     "name VARCHAR(64) NOT NULL, "
     "column_type VARCHAR(512) NOT NULL, "
     "default_value VARCHAR(512) NOT NULL, "
     "nullable enum('NOT NULL', 'NULL') NOT NULL, "
     "array_type enum('FIXED', 'SHORT_VAR', 'MEDIUM_VAR') NOT NULL, "
     "storage_type enum('MEMORY', 'DISK') NOT NULL, "
     "primary_key INT UNSIGNED NOT NULL, "
     "partition_key INT UNSIGNED NOT NULL, "
     "dynamic INT UNSIGNED NOT NULL, "
     "auto_inc INT UNSIGNED NOT NULL"},
    {
        "ndbinfo",
        "dictionary_tables",
        "table_id INT UNSIGNED NOT NULL PRIMARY KEY, "
        "database_name varchar(64) NOT NULL, "
        "table_name varchar(64) NOT NULL, "
        "status enum('New','Changed','Retrieved','Invalid','Altered') NOT "
        "NULL, "
        "attributes INT UNSIGNED NOT NULL, "
        "primary_key_cols INT UNSIGNED NOT NULL, "
        "primary_key VARCHAR(64) NOT NULL, "
        "`storage` enum('memory','disk','default') NOT NULL, "
        "`logging` INT UNSIGNED NOT NULL, "
        "`dynamic` INT UNSIGNED NOT NULL, "
        "read_backup INT UNSIGNED NOT NULL, "
        "fully_replicated INT UNSIGNED NOT NULL, "
        "`checksum` INT UNSIGNED NOT NULL, "
        "`row_size` INT UNSIGNED NOT NULL, "
        "`min_rows` BIGINT UNSIGNED, "
        "`max_rows` BIGINT UNSIGNED, "
        "`tablespace` INT UNSIGNED, "
        "fragment_type enum('Single', 'AllSmall', 'AllMedium','AllLarge',"
        "'DistrKeyHash','DistrKeyLin','UserDefined',"
        "'unused', 'HashMapPartition') NOT NULL, "
        "hash_map VARCHAR(512) NOT NULL, "
        "`fragments` INT UNSIGNED NOT NULL, "
        "`partitions` INT UNSIGNED NOT NULL, "
        "partition_balance VARCHAR(64) NOT NULL, "
        "contains_GCI INT UNSIGNED NOT NULL, "
        "single_user_mode enum('locked','read_only','read_write') NOT NULL, "
        "force_var_part INT UNSIGNED NOT NULL, "
        "GCI_bits INT UNSIGNED NOT NULL, "
        "author_bits INT UNSIGNED NOT NULL",
    },
    {"ndbinfo", "events",
     "event_id INT UNSIGNED NOT NULL PRIMARY KEY, "
     "name varchar(192) NOT NULL, "
     "table_id INT UNSIGNED NOT NULL, "
     "reporting  SET('updated', 'all', 'subscribe', 'DDL') NOT NULL, "
     "columns varchar(512) NOT NULL, "
     "table_event SET('INSERT','DELETE','UPDATE','SCAN','DROP','ALTER',"
     "'CREATE','GCP_COMPLETE','CLUSTER_FAILURE','STOP',"
     "'NODE_FAILURE','SUBSCRIBE','UNSUBSCRIBE','ALL') NOT NULL"},
    {"ndbinfo", "foreign_keys",
     "object_id INT UNSIGNED NOT NULL PRIMARY KEY, "
     "name varchar(140) NOT NULL, "
     "parent_table varchar(140) NOT NULL, "
     "parent_columns varchar(512) NOT NULL, "
     "child_table varchar(140) NOT NULL, "
     "child_columns varchar(512) NOT NULL, "
     "parent_index varchar(140) NOT NULL, "
     "child_index varchar(140) NOT NULL, "
     "on_update_action enum('No Action','Restrict','Cascade','Set Null',"
     "'Set Default') NOT NULL,"
     "on_delete_action enum('No Action','Restrict','Cascade','Set Null',"
     "'Set Default') NOT NULL"},
    {"ndbinfo", "index_columns",
     "table_id int unsigned NOT NULL, "
     "database_name VARCHAR(64) NOT NULL, "
     "table_name VARCHAR(64) NOT NULL, "
     "index_object_id int unsigned NOT NULL, "
     "index_name VARCHAR(64) NOT NULL, "
     "index_type INT UNSIGNED NOT NULL, "
     "status enum('new','changed','retrieved','invalid','altered') NOT NULL, "
     "columns VARCHAR(512) NOT NULL"},
    {"ndbinfo", "ndb$backup_id",
     "id BIGINT UNSIGNED, "
     "fragment INT UNSIGNED, "
     "row_id BIGINT UNSIGNED"},
    {
        "ndbinfo",
        "ndb$blocks",
        "block_number INT UNSIGNED NOT NULL PRIMARY KEY, "
        "block_name VARCHAR(512)",
    },
    {"ndbinfo", "ndb$config_params",
     "param_number INT UNSIGNED NOT NULL PRIMARY KEY, "
     "param_name VARCHAR(512), "
     "param_description VARCHAR(512), "
     "param_type VARCHAR(512), "
     "param_default VARCHAR(512), "
     "param_min VARCHAR(512), "
     "param_max VARCHAR(512), "
     "param_mandatory INT UNSIGNED, "
     "param_status VARCHAR(512)"},
    {
        "ndbinfo",
        "ndb$dblqh_tcconnect_state",
        "state_int_value INT UNSIGNED NOT NULL PRIMARY KEY, "
        "state_name VARCHAR(256), "
        "state_friendly_name VARCHAR(256), "
        "state_description VARCHAR(256)",
    },
    {
        "ndbinfo",
        "ndb$dbtc_apiconnect_state",
        "state_int_value INT UNSIGNED NOT NULL PRIMARY KEY, "
        "state_name VARCHAR(256), "
        "state_friendly_name VARCHAR(256), "
        "state_description VARCHAR(256)",
    },
    {
        "ndbinfo",
        "ndb$dict_obj_types",
        "type_id INT UNSIGNED NOT NULL PRIMARY KEY, "
        "type_name VARCHAR(512)",
    },
    {
        "ndbinfo",
        "ndb$error_messages",
        "error_code INT UNSIGNED, "
        "error_description VARCHAR(512), "
        "error_status VARCHAR(512), "
        "error_classification VARCHAR(512)",
    },
    {
        "ndbinfo",
        "ndb$index_stats",
        "index_id INT UNSIGNED, "
        "index_version INT UNSIGNED, "
        "sample_version INT UNSIGNED",
    }};

static constexpr size_t num_lookups = sizeof(lookups) / sizeof(lookups[0]);

struct obsolete_object {
  const char *schema_name;
  const char *name;
};

/* Views that were present in previous versions */
static struct obsolete_object obsolete_views[] = {
    {"ndbinfo", "dummy_view"}  // replace this with an actual deleted view
};

/* Base tables that were present in previous versions */
static struct obsolete_object obsolete_tables[] = {
    {"ndbinfo", "dummy_table"}  // replace this with an actual deleted table
};

static int compare_names(const void *px, const void *py) {
  const Ndbinfo::Table *const *x =
      static_cast<const Ndbinfo::Table *const *>(px);
  const Ndbinfo::Table *const *y =
      static_cast<const Ndbinfo::Table *const *>(py);
  return strcmp((*x)->m.name, (*y)->m.name);
}

static Plugin_table *ndbinfo_define_table(const Ndbinfo::Table &table) {
  THD *thd = current_thd;  // For string allocation
  BaseString table_name, table_sql, table_options;
  const char *separator = "";

  table_name.assfmt("%s%s", opt_table_prefix, table.m.name);

  for (int j = 0; j < table.m.ncols; j++) {
    const Ndbinfo::Column &col = table.col[j];

    table_sql.appfmt("%s", separator);
    separator = ",";

    table_sql.appfmt("`%s` ", col.name);

    switch (col.coltype) {
      case Ndbinfo::Number:
        table_sql.appfmt("INT UNSIGNED");
        break;
      case Ndbinfo::Number64:
        table_sql.appfmt("BIGINT UNSIGNED");
        break;
      case Ndbinfo::String:
        table_sql.appfmt("VARCHAR(512)");
        break;
      default:
        abort();
    }

    if (col.comment[0] != '\0')
      table_sql.appfmt(" COMMENT \"%s\"", col.comment);
  }

  table_options.appfmt(" COMMENT=\"%s\" ENGINE=NDBINFO CHARACTER SET latin1",
                       table.m.comment);

  return new Plugin_table("ndbinfo", thd_strdup(thd, table_name.c_str()),
                          thd_strdup(thd, table_sql.c_str()),
                          thd_strdup(thd, table_options.c_str()), nullptr);
}

bool ndbinfo_define_dd_tables(List<const Plugin_table> *plugin_tables) {
  /* Drop views from previous versions */
  for (const obsolete_object &v : obsolete_views)
    plugin_tables->push_back(
        new Plugin_view(v.schema_name, v.name, nullptr, nullptr));

  /* Drop base tables from previous versions */
  for (const obsolete_object &t : obsolete_tables)
    plugin_tables->push_back(
        new Plugin_table(t.schema_name, t.name, nullptr, nullptr, nullptr));

  /* Sort Ndbinfo tables; define Ndbinfo tables as tables in DD */
  const Ndbinfo::Table **tables =
      new const Ndbinfo::Table *[Ndbinfo::getNumTables()];

  for (int i = 0; i < Ndbinfo::getNumTables(); i++) {
    tables[i] = &Ndbinfo::getTable(i);
  }
  qsort(tables, Ndbinfo::getNumTables(), sizeof(tables[0]), compare_names);

  for (int i = 0; i < Ndbinfo::getNumTables(); i++)
    plugin_tables->push_back(ndbinfo_define_table(*tables[i]));

  delete[] tables;

  /* Require virtual tables (lookups) defined above to be sorted by name */
  for (size_t i = 0; i < num_lookups; i++)
    assert(i == 0 || strcmp(lookups[i - 1].lookup_table_name,
                            lookups[i].lookup_table_name) < 0);

  /* Create lookup tables in DD */
  for (const lookup &l : lookups)
    plugin_tables->push_back(
        new Plugin_table(l.schema_name, l.lookup_table_name, l.columns,
                         "ENGINE=NDBINFO CHARACTER SET latin1", nullptr));

  /* Require views defined above to be sorted by name */
  for (size_t i = 0; i < num_views; i++)
    assert(i == 0 || strcmp(views[i - 1].view_name, views[i].view_name) < 0);

  /* Create views in DD */
  for (const view &v : views)
    plugin_tables->push_back(
        new Plugin_view(v.schema_name, v.view_name, v.sql,
                        "DEFINER=`root`@`localhost` SQL SECURITY INVOKER"));

  return false;
}
