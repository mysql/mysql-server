
var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_membership_online =
  gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_membership: group_replication_membership_online,
};

// first node is PRIMARY
options.group_replication_primary_member = options.group_replication_membership[0][0];

var router_select_metadata =
  common_stmts.get("router_select_metadata", options);
var router_select_group_membership_with_primary_mode =
  common_stmts.get("router_select_group_membership_with_primary_mode", options);
var router_select_group_replication_primary_member =
  common_stmts.get("router_select_group_replication_primary_member", options);

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses([
  "select_port",
  "router_select_schema_version",
], options);

if (mysqld.global.MD_failed == undefined) {
  mysqld.global.MD_failed = false;
}
if (mysqld.global.GR_primary_failed == undefined) {
  mysqld.global.GR_primary_failed = false;
}
if (mysqld.global.GR_health_failed == undefined) {
  mysqld.global.GR_health_failed = false;
}
({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if (stmt === router_select_metadata.stmt) {
      if (!mysqld.global.MD_failed) {
        return router_select_metadata;
      }
      else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
    }
    else if (stmt === router_select_group_replication_primary_member.stmt) {
      if (!mysqld.global.GR_primary_failed) {
        return router_select_group_replication_primary_member;
      }
      else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
    }
    else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
      if (!mysqld.global.GR_health_failed) {
        return router_select_group_membership_with_primary_mode;
      }
      else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
    }
    else {
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
