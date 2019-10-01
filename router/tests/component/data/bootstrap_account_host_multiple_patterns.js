var common_stmts = require("common_statements");

({
  stmts: function (stmt) {
    var res;

    var options = {};

    var common_responses = common_stmts.prepare_statement_responses([
      "router_select_schema_version",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_select_metadata",
      "router_count_clusters_and_replicasets",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances",
      "router_start_transaction",
      "router_commit",
    ], options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex([
      "router_select_hosts",
      "router_insert_into_hosts",
      "router_insert_into_routers",
      "router_create_user_if_not_exists",
      "router_grant_on_metadata_db",
      "router_update_routers_in_metadata",
    ], options);

    var router_grant_on_pfs_db = common_stmts.prepare_statement_responses_regex([
      "router_grant_on_pfs_db",
    ], options);

    var cu_regex = "CREATE USER IF NOT EXISTS "
                 + "'mysql_router1_.*'@'.*' IDENTIFIED WITH mysql_native_password AS '.*',"
                 + "'mysql_router1_.*'@'.*' IDENTIFIED WITH mysql_native_password AS '.*',"
                 + "'mysql_router1_.*'@'.*' IDENTIFIED WITH mysql_native_password AS '.*'";

    if (stmt.match(cu_regex)) {
      return {"ok": {}};
    }
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, router_grant_on_pfs_db)) !== undefined) {
      return res;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
