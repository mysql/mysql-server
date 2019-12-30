/**
 * run 4 nodes on the current host
 *
 * - 1 PRIMARY
 * - 3 SECONDARY
 *
 * via HTTP interface
 *
 * - primary_failover
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var nodes = function(host, port_and_state) {
  return port_and_state.map(function (current_value) {
    return [ current_value[0], host, current_value[0], current_value[1], current_value[2]];
  });
};

// all nodes are online
var group_replication_membership_online =
  nodes(gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_membership: group_replication_membership_online,
};

// in the startup case, first node is PRIMARY
options.group_replication_primary_member = options.group_replication_membership[0][0];

// in case of failover, announce the 2nd node as PRIMARY
var options_failover = Object.assign({}, options,
  { group_replication_primary_member: options.group_replication_membership[1][0] });

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses([
  "select_port",
  "router_select_schema_version",
  "router_select_metadata",
  "router_select_group_membership_with_primary_mode",
], options);

// allow to switch
var router_select_group_replication_primary_member
  = common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_replication_primary_member_failover
  = common_stmts.get("router_select_group_replication_primary_member", options_failover);

if (mysqld.global.primary_failover == undefined) {
  mysqld.global.primary_failover = false;
}

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_group_replication_primary_member.stmt) {
      if (!mysqld.global.primary_failover) {
        return router_select_group_replication_primary_member;
      } else {
        return router_select_group_replication_primary_member_failover;
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
