var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

// all nodes are online
var group_replication_membership_online =
    gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

// create a partioned membership result
//
// last 3 nodes are UNREACHABLE
var group_replication_membership_partitioned =
    group_replication_membership_online.map(function(v, ndx) {
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
  group_replication_membership: group_replication_membership_online,
  cluster_type: "gr",
};
options.group_replication_primary_member =
    options.group_replication_membership[0][0];

var router_select_group_membership_with_primary_mode = common_stmts.get(
    "router_select_group_membership_with_primary_mode", options);

var router_select_group_membership_with_primary_mode_partitioned =
    common_stmts.get(
        "router_select_group_membership_with_primary_mode",
        Object.assign(options, {
          group_replication_membership: group_replication_membership_partitioned
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
      "router_select_cluster_type_v2",
      "router_select_metadata_v2_gr",
      "router_select_group_replication_primary_member",
      "router_update_last_check_in_v2",
      "router_clusterset_present",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_update_attributes_v2",
    ],
    options);


if (mysqld.global.cluster_partition === undefined) {
  mysqld.global.cluster_partition = false;
}

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_select_group_membership_with_primary_mode.stmt) {
      if (!mysqld.global.cluster_partition) {
        return router_select_group_membership_with_primary_mode;
      } else {
        return router_select_group_membership_with_primary_mode_partitioned;
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
