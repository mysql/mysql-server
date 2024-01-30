var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = [];
}


var options = {
  gr_id: mysqld.global.gr_id,
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_memberships.gr_members(
      mysqld.global.gr_node_host, mysqld.global.gr_nodes),
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
  router_version: mysqld.global.router_version,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_select_replication_group_name",
      "router_select_cluster_id_v2_ar",
      "router_select_view_id_bootstrap_ar",
      "router_commit",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_select_router_address",
      "router_insert_into_routers",
      "router_create_user_if_not_exists",
      "router_check_auth_plugin",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_v2_routers",
      "router_grant_on_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
      "router_select_config_defaults_stored_gr_cluster",
      "router_select_config_defaults_stored_ar_cluster",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
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
