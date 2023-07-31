/**
 * run 4 nodes on the current host
 *
 * - 1 PRIMARY
 * - 3 SECONDARY
 *
 * via HTTP interface
 *
 * - MD_asked
 * - health_asked
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_members_online =
    gr_memberships.gr_members(gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
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
      "router_check_member_state",
      "router_select_members_count",
      "router_select_router_options_view",
    ],
    options);

// track the statements directly to allow the HTTP interface to query
// if they have been executed ("asked")
var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options);
var router_select_group_membership =
    common_stmts.get("router_select_group_membership", options);


if (mysqld.global.MD_asked === undefined) {
  mysqld.global.MD_asked = false;
}

if (mysqld.global.health_asked === undefined) {
  mysqld.global.health_asked = false;
}

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.MD_asked = true;
      return router_select_metadata;
    } else if (stmt === router_select_group_membership.stmt) {
      mysqld.global.health_asked = true;
      return router_select_group_membership;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
