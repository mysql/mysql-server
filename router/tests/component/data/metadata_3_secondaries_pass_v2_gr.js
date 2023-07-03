/**
 * run 4 nodes on the current host
 *
 * - 1 PRIMARY
 * - 3 SECONDARY
 *
 * via HTTP interface
 *
 * - MD_asked
 * - primary_asked
 * - health_asked
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_membership_online =
    gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_membership: group_replication_membership_online,
  cluster_type: "gr",
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership[0][0];

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
    ],
    options);

// track the statements directly to allow the HTTP interface to query
// if they have been executed ("asked")
var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options);
var router_select_group_replication_primary_member =
    common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_membership_with_primary_mode = common_stmts.get(
    "router_select_group_membership_with_primary_mode", options);


if (mysqld.global.MD_asked === undefined) {
  mysqld.global.MD_asked = false;
}

if (mysqld.global.primary_asked === undefined) {
  mysqld.global.primary_asked = false;
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
    } else if (stmt === router_select_group_replication_primary_member.stmt) {
      mysqld.global.primary_asked = true;
      return router_select_group_replication_primary_member;
    } else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
      mysqld.global.health_asked = true;
      return router_select_group_membership_with_primary_mode;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
