var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

// all nodes are online
var group_replication_members_online =
    gr_memberships.gr_members(gr_node_host, mysqld.global.gr_nodes);

// create a partioned membership result
//
// last 3 nodes are UNREACHABLE
var group_replication_members_partitioned =
    group_replication_members_online.map(function(v, ndx) {
      if (ndx >= 2) {
        return v.map(function(v_, ndx_) {
          if (ndx_ == 3) {
            return "UNREACHABLE";
          } else {
            return v_;
          }
        });
      } else {
        return v;
      }
    });

var options = {
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  metadata_schema_version: [1, 0, 2],
  gr_id: mysqld.global.gr_id,
};

var router_select_metadata =
    common_stmts.get("router_select_metadata", options);
var router_select_group_membership =
    common_stmts.get("router_select_group_membership", options);

var router_select_group_membership_partitioned =
    common_stmts.get("router_select_group_membership", Object.assign(options, {
      group_replication_members: group_replication_members_partitioned
    }));

// common stmts

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_check_member_state",
      "router_select_members_count",
    ],
    options);

if (mysqld.global.cluster_partition === undefined) {
  mysqld.global.cluster_partition = false;
}

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_metadata.stmt) {
      return router_select_metadata;
    } else if (stmt === router_select_group_membership.stmt) {
      if (!mysqld.global.cluster_partition) {
        return router_select_group_membership;
      } else {
        return router_select_group_membership_partitioned;
      }
    } else {
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Errorxxxx at: " + stmt
        }
      };
    }
  }
})
