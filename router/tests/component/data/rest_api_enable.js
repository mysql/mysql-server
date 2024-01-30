var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.innodb_cluster_instances === undefined) {
  mysqld.global.innodb_cluster_instances = [];
}

if (mysqld.global.schema_version === undefined) {
  mysqld.global.schema_version = [2, 0, 0];
}

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = [];
}
var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
  innodb_cluster_name: "mycluster",
  gr_id: mysqld.global.gr_id,
  metadata_schema_version: mysqld.global.schema_version,
  replication_group_members: gr_memberships.gr_members(
      mysqld.global.gr_node_host, mysqld.global.gr_nodes),
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  router_version: mysqld.global.router_version,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_current_instance_attributes",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_start_transaction",
      "router_commit",
      "router_select_rest_accounts_credentials_gr_by_uuid",
      "router_clusterset_present",

      // to fail account verification in some tests this is not added on
      // purpose
      "router_select_metadata_v2_gr",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_group_membership",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_create_user_if_not_exists",
      "router_check_auth_plugin",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
      "router_select_config_defaults_stored_gr_cluster",
    ],
    options);


({
  stmts: function(stmt) {
    var res;
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
