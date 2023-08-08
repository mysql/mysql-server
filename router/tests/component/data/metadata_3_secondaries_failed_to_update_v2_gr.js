
var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var group_replication_members_online = gr_memberships.gr_members(
    mysqld.global.gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  metadata_schema_version: [1, 0, 2],
};

var router_select_metadata =
    common_stmts.get("router_select_metadata", options);
var router_select_group_membership =
    common_stmts.get("router_select_group_membership", options);

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_clusterset_present",
      "router_check_member_state",
      "router_select_members_count",
    ],
    options);

if (mysqld.global.MD_failed === undefined) {
  mysqld.global.MD_failed = false;
}
if (mysqld.global.GR_primary_failed === undefined) {
  mysqld.global.GR_primary_failed = false;
}
if (mysqld.global.GR_health_failed === undefined) {
  mysqld.global.GR_health_failed = false;
}
({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_metadata.stmt) {
      if (!mysqld.global.MD_failed) {
        return router_select_metadata;
      } else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
    } else if (stmt === router_select_group_membership.stmt) {
      if (!mysqld.global.GR_health_failed) {
        return router_select_group_membership;
      } else {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        };
      }
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
