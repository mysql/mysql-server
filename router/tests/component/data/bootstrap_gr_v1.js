var common_stmts = require("common_statements");

var options = {
  metadata_schema_version: [1, 0, 2],
  cluster_type: "gr",
  innodb_cluster_name: "mycluster",
  innodb_cluster_instances:
      [["localhost", 5500], ["localhost", 5510], ["localhost", 5520]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_count_clusters_v1",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v1",
      "router_start_transaction",
      "router_commit",

      // account verification
      "router_select_metadata",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_hosts_v1",
      "router_insert_into_routers_v1",  // TODO: check which are needed
      "router_select_hosts_v1",
      "router_select_hosts_join_routers_v1",
      "router_create_user_if_not_exists",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_update_routers_in_metadata_v1",
    ],
    options);

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
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
