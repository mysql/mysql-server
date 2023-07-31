var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if (mysqld.global.innodb_cluster_name === undefined) {
  mysqld.global.innodb_cluster_name = "test";
}


var options = {
  metadata_schema_version: [2, 1, 0],
  innodb_cluster_name: mysqld.global.innodb_cluster_name,
  gr_id: mysqld.global.gr_id,
  group_replication_members:
      gr_memberships.gr_members(gr_node_host, mysqld.global.gr_nodes),
  innodb_cluster_instances:
      gr_memberships.cluster_nodes(gr_node_host, mysqld.global.cluster_nodes),
};

var options_primary_unavailable = {
  group_replication_members:
      options.group_replication_members.filter(function(el, ndx) {
        return ndx != 0
      })
};

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_clusterset_present",
      "router_select_metadata_v2_gr",
      "router_check_member_state",
      "router_select_members_count",
    ],
    options);

var router_select_group_membership_primary_unavailable = common_stmts.get(
    "router_select_group_membership", options_primary_unavailable);


({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        stmt === router_select_group_membership_primary_unavailable.stmt) {
      return router_select_group_membership_primary_unavailable;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
