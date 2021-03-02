var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.innodb_cluster_instances === undefined) {
  mysqld.global.innodb_cluster_instances = [];
}

if (mysqld.global.schema_version === undefined) {
  mysqld.global.schema_version = [2, 0, 0];
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0], host, current_value[0], current_value[1],
      current_value[2]
    ];
  });
};

({
  stmts: function(stmt) {
    var options = {
      cluster_type: "gr",
      innodb_cluster_name: "mycluster",
      innodb_cluster_instances: mysqld.global.innodb_cluster_instances,
      gr_id: mysqld.global.gr_id,
      group_replication_name: mysqld.global.gr_id,
      metadata_schema_version: mysqld.global.schema_version,
    };

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
          "router_start_transaction",
          "router_commit",
          "router_select_rest_accounts_credentials",

          // to fail account verification in some tests this is not added on
          // purpose "router_select_metadata_v2_gr",
          "router_select_group_replication_primary_member",
          "router_select_group_membership_with_primary_mode",
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
        ],
        options);

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
