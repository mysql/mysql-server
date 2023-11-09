/**
 * run 1 node on the current host
 *
 * - 1 PRIMARY
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_members_online =
    gr_memberships.single_host(gr_node_host, [
      [mysqld.session.port, "ONLINE", "PRIMARY"],
    ]);

var cluster_nodes = gr_memberships.single_host_cluster_nodes(
    gr_node_host, [[mysqld.session.port]], "uuid");

var options = {
  metadata_schema_version: [1, 0, 1],
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: cluster_nodes,
};

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_commit",
      "select_port",
      "router_select_schema_version",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_group_membership",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
