var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

// all nodes are online
var group_replication_membership_online =
  gr_memberships.nodes(gr_node_host, mysqld.global.gr_nodes);

// create a partioned membership result
//
// last 3 nodes are UNREACHABLE
var group_replication_membership_partitioned = group_replication_membership_online.map(function(v, ndx) {
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
};
options.group_replication_primary_member = options.group_replication_membership[0][0];

var router_select_metadata =
  common_stmts.get("router_select_metadata", options);
var router_select_group_replication_primary_member =
  common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_membership_with_primary_mode =
  common_stmts.get("router_select_group_membership_with_primary_mode", options);

var router_select_group_membership_with_primary_mode_partitioned =
  common_stmts.get("router_select_group_membership_with_primary_mode",
    Object.assign(options, { group_replication_membership: group_replication_membership_partitioned }));

// common stmts

var router_select_schema_version = common_stmts.get("router_select_schema_version");
var select_port = common_stmts.get("select_port");

if (mysqld.global.cluster_partition === undefined) {
  mysqld.global.cluster_partition = false;
}

({
  stmts: function (stmt) {
    if (stmt === router_select_schema_version.stmt) {
      return router_select_schema_version;
    } else if (stmt === select_port.stmt) {
      return select_port;
    } else if (stmt === router_select_metadata.stmt) {
      return router_select_metadata;
    } else if (stmt === router_select_group_replication_primary_member.stmt) {
      return router_select_group_replication_primary_member;
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
