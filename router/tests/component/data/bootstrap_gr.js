var common_stmts = require("common_statements");

if (mysqld.global.innodb_cluster_instances === undefined) {
  mysqld.global.innodb_cluster_instances =
      [["localhost", 5500], ["localhost", 5510], ["localhost", 5520]];
}

if (mysqld.global.cluster_name == undefined) {
  mysqld.global.cluster_name = "my-cluster";
}

if (mysqld.global.metadata_version === undefined) {
  mysqld.global.metadata_version = [2, 0, 3];
}

var options = {
  metadata_schema_version: mysqld.global.metadata_version,
  cluster_type: "gr",
  clusterset_present: 0,
  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: mysqld.global.innodb_cluster_instances,
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

      // account verification
      "router_select_metadata_v2_gr",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
    ],
    options);

var common_responses_v2_1 = common_stmts.prepare_statement_responses(
    [
      "router_clusterset_present",
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
    }
    // metadata ver 2.1+
    else if (
        (mysqld.global.metadata_version[0] >= 2 &&
         mysqld.global.metadata_version[1] >= 1) &&
        common_responses_v2_1.hasOwnProperty(stmt)) {
      return common_responses_v2_1[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
