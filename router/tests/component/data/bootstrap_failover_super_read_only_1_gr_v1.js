var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "CLUSTER-ID";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = [];
}

var options = {
  metadata_schema_version: [1, 0, 2],
  gr_id: mysqld.global.gr_id,
  cluster_type: "gr",
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_memberships.gr_members(
      mysqld.global.gr_node_host, mysqld.global.gr_nodes),
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
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
      "router_replication_group_members",
      "router_start_transaction",
      "router_commit",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_select_hosts_v1",
    ],
    options);

var router_insert_into_hosts =
    common_stmts.get("router_insert_into_hosts_v1", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_insert_into_hosts.stmt_regex)) {
      return {
        error: {
          code: 1290,
          sql_state: "HY001",
          message:
              "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
