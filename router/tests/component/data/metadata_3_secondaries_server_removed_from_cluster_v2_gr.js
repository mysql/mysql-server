/**
 * create a cluster with 4 nodes
 *
 * - 1 primary
 * - 3 secondary
 *
 * either PRIMARY or the first SECONDARY can be disabled through the HTTP
 * interface
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

// all nodes are online
var group_replication_membership_online =
    gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_membership: group_replication_membership_online,
  cluster_type: "gr",
};
options.group_replication_primary_member =
    options.group_replication_membership[0][0];

var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options);
var router_select_group_replication_primary_member =
    common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_membership_with_primary_mode = common_stmts.get(
    "router_select_group_membership_with_primary_mode", options);


// primary is removed, first secondary is the new PRIMARY
var options_removed_primary = {
  group_replication_membership:
      group_replication_membership_online.filter(function(el, ndx) {
        return ndx != 0
      }),
  cluster_type: "gr",
};
options_removed_primary.group_replication_primary_member =
    options_removed_primary.group_replication_membership[0][0];

var router_select_metadata_removed_primary =
    common_stmts.get("router_select_metadata_v2_gr", options_removed_primary);
var router_select_group_replication_primary_member_removed_primary =
    common_stmts.get(
        "router_select_group_replication_primary_member",
        options_removed_primary);
var router_select_group_membership_with_primary_mode_removed_primary =
    common_stmts.get(
        "router_select_group_membership_with_primary_mode",
        options_removed_primary);


// first secondary is removed, PRIMARY stays PRIMARY
var options_removed_secondary = {
  group_replication_membership:
      group_replication_membership_online.filter(function(el, ndx) {
        return ndx != 1
      }),
  cluster_type: "gr",
};
options_removed_secondary.group_replication_primary_member =
    options_removed_secondary.group_replication_membership[0][0];

var router_select_metadata_removed_secondary =
    common_stmts.get("router_select_metadata_v2_gr", options_removed_secondary);
var router_select_group_replication_primary_member_removed_secondary =
    common_stmts.get(
        "router_select_group_replication_primary_member",
        options_removed_secondary);
var router_select_group_membership_with_primary_mode_removed_secondary =
    common_stmts.get(
        "router_select_group_membership_with_primary_mode",
        options_removed_secondary);


// common queries

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_clusterset_present",
    ],
    options);

var select_port = common_stmts.get("select_port");

if (mysqld.global.primary_removed === undefined) {
  mysqld.global.primary_removed = false;
}

if (mysqld.global.secondary_removed === undefined) {
  mysqld.global.secondary_removed = false;
}

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === select_port.stmt) {
      return select_port;
    } else if (stmt === router_select_metadata.stmt) {
      if (mysqld.global.secondary_removed) {
        return router_select_metadata_removed_secondary;
      } else if (mysqld.global.primary_removed) {
        return router_select_metadata_removed_primary;
      } else {
        return router_select_metadata;
      }
    } else if (stmt === router_select_group_replication_primary_member.stmt) {
      if (mysqld.global.secondary_removed) {
        return router_select_group_replication_primary_member_removed_secondary;
      } else if (mysqld.global.primary_removed) {
        return router_select_group_replication_primary_member_removed_primary;
      } else {
        return router_select_group_replication_primary_member;
      }
    } else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
      if (mysqld.global.secondary_removed) {
        return router_select_group_membership_with_primary_mode_removed_secondary;
      } else if (mysqld.global.primary_removed) {
        return router_select_group_membership_with_primary_mode_removed_primary;
      } else {
        return router_select_group_membership_with_primary_mode;
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
