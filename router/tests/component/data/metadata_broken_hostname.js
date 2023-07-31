var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

// start with a valid hostname to startup nicely, but change the GR node
// hostname to something broken on the 2nd connection.
var gr_node_host = "127.0.0.1";

if (mysqld.global.changed === undefined) {
  mysqld.global.changed = false;
} else {
  gr_node_host = "[broken]";
}

var group_replication_members_online = gr_memberships.single_host(
    gr_node_host, [[mysqld.session.port, "ONLINE", "PRIMARY"]], "uuid");

var cluster_nodes = gr_memberships.single_host_cluster_nodes(
    gr_node_host, [[mysqld.session.port]], "uuid");

var options = {
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: cluster_nodes,
  gr_id: "uuid"
};

var common_responses = common_stmts.prepare_callable_statement_responses(
    [
      "select_length_4097",
      "select_repeat_4097",
      "router_show_cipher_status",
      "router_show_mysqlx_cipher_status",
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_metadata_v2_gr",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_group_membership",
      "router_update_last_check_in_v2",
      "router_clusterset_present",
      "router_select_router_options_view",
      "router_commit",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt]();
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
