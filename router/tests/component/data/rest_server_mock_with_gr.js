var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [[mysqld.session.port, "ONLINE"]];
}

var group_replication_members_online =
    gr_memberships.gr_members(gr_node_host, mysqld.global.gr_nodes);

// allow to change the connect_exec_time before the greeting of the server is
// sent
var connect_exec_time = (mysqld.global.connect_exec_time === undefined) ?
    0 :
    mysqld.global.connect_exec_time;

var options = {
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  cluster_type: "gr",
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_members[0][0];

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_update_last_check_in_v2",
      "router_clusterset_present",
    ],
    options);

({
  handshake: {
    greeting: {exec_time: connect_exec_time},
    auth: {username: mysqld.global.username, password: mysqld.global.password}
  },
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else {
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      };
    }
  }
})
