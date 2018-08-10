/**
 * create a cluster with 4 nodes
 *
 * - 1 primary
 * - 3 secondary
 *
 * either PRIMARY or the first SECONDARY can be disabled through the HTTP interface
 */

var common_stmts = require("common_statements");

var group_replication_membership = [
  [
    "37dbb0e3-cfc0-11e7-8039-080027d01fcd",
    "127.0.0.1",
    process.env.PRIMARY_PORT,
    "ONLINE",
  ],
  [
    "49cff431-cfc0-11e7-bb87-080027d01fcd",
    "127.0.0.1",
    process.env.SECONDARY_1_PORT,
    "ONLINE",
  ],
  [
    "56d0f99d-cfc0-11e7-bb0a-080027d01fcd",
    "127.0.0.1",
    process.env.SECONDARY_2_PORT,
    "ONLINE",
  ],
  [
    "6689460c-cfc0-11e7-907b-080027d01fcd",
    "127.0.0.1",
    process.env.SECONDARY_3_PORT,
    "ONLINE",
  ],
];


// all nodes are online
var options = {
  group_replication_membership: group_replication_membership,
};
options.group_replication_primary_member = options.group_replication_membership[0][0];

var router_select_metadata =
  common_stmts.get("router_select_metadata", options);
var router_select_group_replication_primary_member =
  common_stmts.get("router_select_group_replication_primary_member", options);
var router_select_group_membership_with_primary_mode =
  common_stmts.get("router_select_group_membership_with_primary_mode", options);


// primary is removed, first secondary is the new PRIMARY
var options_removed_primary = {
  group_replication_membership: group_replication_membership.filter(function(el, ndx) { return ndx != 0 })
};
options_removed_primary.group_replication_primary_member = options_removed_primary.group_replication_membership[0][0];

var router_select_metadata_removed_primary =
  common_stmts.get("router_select_metadata", options_removed_primary);
var router_select_group_replication_primary_member_removed_primary =
  common_stmts.get("router_select_group_replication_primary_member", options_removed_primary);
var router_select_group_membership_with_primary_mode_removed_primary =
  common_stmts.get("router_select_group_membership_with_primary_mode", options_removed_primary);


// first secondary is removed, PRIMARY stays PRIMARY
var options_removed_secondary = {
  group_replication_membership: group_replication_membership.filter(function(el, ndx) { return ndx != 1 })
};
options_removed_secondary.group_replication_primary_member = options_removed_secondary.group_replication_membership[0][0];

var router_select_metadata_removed_secondary =
  common_stmts.get("router_select_metadata", options_removed_secondary);
var router_select_group_replication_primary_member_removed_secondary =
  common_stmts.get("router_select_group_replication_primary_member", options_removed_secondary);
var router_select_group_membership_with_primary_mode_removed_secondary =
  common_stmts.get("router_select_group_membership_with_primary_mode", options_removed_secondary);


// common queries

var router_select_schema_version = common_stmts.get("router_select_schema_version");
var select_port = common_stmts.get("select_port");

if (mysqld.global.primary_removed == undefined) {
  mysqld.global.primary_removed = false;
}

if (mysqld.global.secondary_removed == undefined) {
  mysqld.global.secondary_removed = false;
}

({
  stmts: function (stmt) {
    if (stmt === router_select_schema_version.stmt) {
      return router_select_schema_version;
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
