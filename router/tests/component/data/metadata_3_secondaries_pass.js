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
  gr_memberships.single_host(gr_node_host, [
    [ process.env.PRIMARY_PORT, "ONLINE" ],
    [ process.env.SECONDARY_1_PORT, "ONLINE" ],
    [ process.env.SECONDARY_2_PORT, "ONLINE" ],
    [ process.env.SECONDARY_3_PORT, "ONLINE" ],
  ]);

var options = {
  group_replication_membership: group_replication_membership_online,
};

// first node is PRIMARY
options.group_replication_primary_member = options.group_replication_membership[0][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses([
  "select_port",
  "router_select_schema_version",
], options);

// track the statements directly to allow the HTTP interface to query
// if they have been executed ("asked")
var router_select_metadata =
  common_stmts.get("router_select_metadata", options);
var router_select_group_replication_primary_member =
  common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_membership_with_primary_mode =
  common_stmts.get("router_select_group_membership_with_primary_mode", options);


if (mysqld.global.MD_asked == undefined) {
  mysqld.global.MD_asked = false;
}

if (mysqld.global.primary_asked == undefined) {
  mysqld.global.primary_asked = false;
}

if (mysqld.global.health_asked == undefined) {
  mysqld.global.health_asked = false;
}

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if (stmt === router_select_metadata.stmt) {
      mysqld.global.MD_asked = true;
      return router_select_metadata;
    }
    else if (stmt === router_select_group_replication_primary_member.stmt) {
      mysqld.global.primary_asked = true;
      return router_select_group_replication_primary_member;
    }
    else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
        mysqld.global.health_asked = true;
      return router_select_group_membership_with_primary_mode;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
