var common_stmts = require("common_statements");

var options = {
  innodb_cluster_name: "test",
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_select_metadata",
      "router_count_clusters_and_replicasets",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_start_transaction",
      "router_commit",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_create_user_if_not_exists",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
    ],
    options);

var router_select_cluster_instances = common_stmts.prepare_statement_responses(
    ["router_select_cluster_instances"], options);

({
  stmts: function(stmt) {
    var res;
    if (router_select_cluster_instances.hasOwnProperty(stmt)) {
      // lets add some fake field with big data
      var result = router_select_cluster_instances[stmt];
      result.result.columns.push({"type": "STRING", "name": "fake"});
      var first = true;
      var rows_qty = result.result.rows.length;
      for (var i = 0; i < rows_qty; i++) {
        if (first) {
          result.result.rows[i].push("x".repeat(4194304));
        } else {
          row.push("");
        }
      }
      return result;
    } else if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
