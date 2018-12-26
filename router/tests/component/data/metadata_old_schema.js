/**
 * run 1 node on the current host
 *
 * - 1 PRIMARY
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_membership_online =
  gr_memberships.single_host(gr_node_host, [
    [ mysqld.session.port, "ONLINE" ],
  ]);

var options = {
  metadata_schema_version: [0, 0, 0],
  group_replication_membership: group_replication_membership_online,
};

// first node is PRIMARY
options.group_replication_primary_member = options.group_replication_membership[0][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses([
  "select_port",
  "router_select_schema_version",
  "router_select_metadata",
  "router_select_group_replication_primary_member",
  "router_select_group_membership_with_primary_mode",
], options);

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
