var common_stmts = require("common_statements");

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
}

if (mysqld.global.update_last_check_in_count === undefined) {
  mysqld.global.update_last_check_in_count = 0;
}

var options = {
  cluster_type: "gr",
  metadata_schema_version: [2, 1, 0],
  clusterset_present: 1,
  bootstrap_target_type: "clusterset",
  clusterset_target_cluster_id: mysqld.global.target_cluster_id,
  view_id: mysqld.global.view_id,
  clusterset_data: mysqld.global.clusterset_data,
  router_options: mysqld.global.router_options,
  clusterset_simulate_cluster_not_found:
      mysqld.global.simulate_cluster_not_found,
};

// TODO: clean those not needed here
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2",
      "router_commit",
      "router_router_options",
      "router_rollback",

      "select_port",

      // clusterset specific
      "router_clusterset_view_id",
      "router_clusterset_all_nodes_by_clusterset_id",
      "router_clusterset_present",
      "router_bootstrap_target_type",
      "router_clusterset_select_cluster_info_by_primary_role",
      "router_clusterset_select_cluster_info_by_gr_uuid",
      "router_clusterset_select_gr_primary_member",
      "router_clusterset_select_gr_members_status",
    ],
    options);


var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_unknown_clusterset_view_id",
      "router_clusterset_select_cluster_info_by_gr_uuid_unknown",
      "router_clusterset_id",
    ],
    options);

var router_start_transaction =
    common_stmts.get("router_start_transaction", options);

var router_update_attributes =
    common_stmts.get("router_update_attributes_v2", options);

var router_update_last_check_in =
    common_stmts.get("router_update_last_check_in_v2", options);


({
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_update_last_check_in.stmt) {
      mysqld.global.update_last_check_in_count++;
      return router_update_last_check_in;
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_update_attributes.stmt_regex)) {
      mysqld.global.update_attributes_count++;
      return router_update_attributes;
    } else if (stmt === "set @@mysqlx_wait_timeout = 28800") {
      return {
        ok: {}
      }
    } else if (stmt === "enable_notices") {
      return {
        ok: {}
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
