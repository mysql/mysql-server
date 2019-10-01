var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";


// all nodes are online
var group_replication_membership_online =
  gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

var options = {
  metadata_schema_version: [1, 0, 2],
  group_replication_membership: group_replication_membership_online,
};

var options_primary_unavailable = {
  group_replication_membership: group_replication_membership_online.filter(function(el, ndx) { return ndx != 0 })
};

// first node is PRIMARY
options.group_replication_primary_member = options.group_replication_membership[0][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses([
  "select_port",
  "router_start_transaction",
  "router_commit",
  "router_select_schema_version",
  "router_select_metadata",
], options);

var router_select_group_membership_primary_unavailable =
  common_stmts.get("router_select_group_membership_with_primary_mode", options_primary_unavailable);


({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
        return router_select_group_membership_primary_unavailable;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})

