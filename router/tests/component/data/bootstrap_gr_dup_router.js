var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");


if (mysqld.global.cluster_name == undefined) {
  mysqld.global.cluster_name = "mycluster";
}

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  replication_group_members: gr_memberships.gr_members(
      mysqld.global.gr_node_host, mysqld.global.gr_nodes),
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
      "router_clusterset_present",
      "router_replication_group_members",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_select_router_id",
      "router_create_user_if_not_exists",
      "router_check_auth_plugin",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_router_options_in_metadata",
      "router_select_config_defaults_stored_gr_cluster",
      "router_update_routers_in_metadata",
    ],
    options);

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
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_insert_into_routers.stmt_regex)) {
      return {
        error: {
          code: 1062,
          sql_state: "HY001",
          message: "Duplicate entry 'xxx' for key 'routers.address'"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
