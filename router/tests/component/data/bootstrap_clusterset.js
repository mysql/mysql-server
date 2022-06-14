var common_stmts = require("common_statements");

if (mysqld.global.session_count === undefined) {
  mysqld.global.session_count = 0;
}

var options = {
  cluster_type: "gr",

  metadata_schema_version: [2, 1, 0],
  clusterset_present: 1,
  clusterset_target_cluster_id: mysqld.global.target_cluster_id,
  clusterset_data: mysqld.global.clusterset_data,
  clusterset_data: mysqld.global.clusterset_data,
  clusterset_simulate_cluster_not_found:
      mysqld.global.simulate_cluster_not_found,
  router_expected_target_cluster: mysqld.global.router_expected_target_cluster,
  group_replication_name:
      mysqld.global.clusterset_data
          .clusters[mysqld.global.clusterset_data.this_cluster_id]
          .gr_uuid,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2",
      "router_start_transaction",
      "router_commit",
      "router_rollback",

      // account verification
      //"router_select_metadata_v2_gr",
      //"router_select_group_replication_primary_member",
      //"router_select_group_membership_with_primary_mode",

      // clusterset specific
      "router_clusterset_cluster_info_by_name",
      "router_clusterset_cluster_info_current_cluster",
      "router_clusterset_cluster_info_primary",
      "router_clusterset_all_nodes",
      "router_clusterset_present",
      "router_clusterset_id_current",
      "router_clusterset_view_id",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_create_user_if_not_exists",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_clusterset_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
      "router_clusterset_cluster_info_by_name_unknown",
    ],
    options);

var router_set_session_options =
    common_stmts.get("router_set_session_options", options);

var router_insert_into_routers =
    common_stmts.get("router_insert_into_routers", options);

({
  handshake: {
    auth: {
      username: "root",
      password: "fake-pass",
    }
  },
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_set_session_options.stmt) {
      mysqld.global.session_count++;
      return router_set_session_options;
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_insert_into_routers.stmt_regex)) {
      var is_primary = options.clusterset_data
                           .clusters[options.clusterset_data.this_cluster_id]
                           .role === "PRIMARY";
      if (is_primary) {
        return {"ok": {"last_insert_id": 1}};
      } else {
        return {
          error: {
            code: 1290,
            sql_state: "HY001",
            message:
                "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
          }
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
