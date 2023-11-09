var common_stmts = require("common_statements");

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_current_instance_attributes",
      "router_select_group_membership",
      "router_select_metadata_v2_gr",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
      "router_clusterset_present",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_create_user_if_not_exists",
      "router_delete_old_accounts",
      "router_create_user",
      "router_check_auth_plugin",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
    ],
    options);

var router_grant_on_metadata_db =
    common_stmts.get("router_grant_on_metadata_db", options);


({
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_grant_on_metadata_db.stmt_regex)) {
      return {
        error: {
          code: 1044,
          sql_state: "HY001",
          message:
              "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
