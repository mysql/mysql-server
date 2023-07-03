/**
 * run 1 node on the current host
 *
 * - 1 PRIMARY
 *
 * via HTTP interface
 *
 * - md_query_count
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.primary_id === undefined) {
  mysqld.global.primary_id = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
}

if (mysqld.global.update_last_check_in_count === undefined) {
  mysqld.global.update_last_check_in_count = 0;
}

if (mysqld.global.router_version === undefined) {
  mysqld.global.router_version = "";
}

if (mysqld.global.router_rw_classic_port === undefined) {
  mysqld.global.router_rw_classic_port = "";
}

if (mysqld.global.router_ro_classic_port === undefined) {
  mysqld.global.router_ro_classic_port = "";
}

if (mysqld.global.router_rw_x_port === undefined) {
  mysqld.global.router_rw_x_port = "";
}

if (mysqld.global.router_ro_x_port === undefined) {
  mysqld.global.router_ro_x_port = "";
}

if (mysqld.global.router_metadata_user === undefined) {
  mysqld.global.router_metadata_user = "";
}

if (mysqld.global.perm_error_on_version_update === undefined) {
  mysqld.global.perm_error_on_version_update = 0;
}

if (mysqld.global.upgrade_in_progress === undefined) {
  mysqld.global.upgrade_in_progress = 0;
}

if (mysqld.global.queries_count === undefined) {
  mysqld.global.queries_count = 0;
}

if (mysqld.global.queries === undefined) {
  mysqld.global.queries = [];
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.clusterset_present === undefined) {
  mysqld.global.clusterset_present = 0;
}

if (mysqld.global.bootstrap_target_type === undefined) {
  mysqld.global.bootstrap_target_type = "cluster";
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0], host, current_value[0], current_value[1],
      current_value[2]
    ];
  });
};

var group_replication_membership_online =
    nodes(gr_node_host, mysqld.global.gr_nodes);

var metadata_version =
    (mysqld.global.upgrade_in_progress === 1) ? [0, 0, 0] : [2, 1, 0];
var options = {
  metadata_schema_version: metadata_version,
  group_replication_membership: group_replication_membership_online,
  gr_id: mysqld.global.gr_id,
  cluster_type: "gr",
  router_version: mysqld.global.router_version,
  router_rw_classic_port: mysqld.global.router_rw_classic_port,
  router_ro_classic_port: mysqld.global.router_ro_classic_port,
  router_rw_x_port: mysqld.global.router_rw_x_port,
  router_ro_x_port: mysqld.global.router_ro_x_port,
  router_metadata_user: mysqld.global.router_metadata_user,
  clusterset_present: mysqld.global.clusterset_present,
  bootstrap_target_type: mysqld.global.bootstrap_target_type,
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership[mysqld.global.primary_id][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_commit",
      "router_rollback",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
      "router_update_last_check_in_v2",
      "router_clusterset_present",
      "router_bootstrap_target_type",
      "router_router_options",
    ],
    options);

var router_update_attributes_strict_v2 =
    common_stmts.get("router_update_attributes_strict_v2", options);

var router_update_last_check_in_v2 =
    common_stmts.get("router_update_last_check_in_v2", options);

var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options);

var router_start_transaction =
    common_stmts.get("router_start_transaction", options);

({
  stmts: function(stmt) {
    // let's grab first queries for the verification
    if (mysqld.global.queries_count < 4) {
      var tmp = mysqld.global.queries;
      tmp.push(stmt)
      mysqld.global.queries = tmp;
      mysqld.global.queries_count++;
    }

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_update_attributes_strict_v2.stmt) {
      mysqld.global.update_attributes_count++;
      if (mysqld.global.perm_error_on_version_update === 1) {
        return {
          error: {
            code: 1142,
            sql_state: "HY001",
            message:
                "UPDATE command denied to user 'user'@'localhost' for table 'v2_routers'"
          }
        }
      } else
        return router_update_attributes_strict_v2;
    } else if (stmt === router_update_last_check_in_v2.stmt) {
      mysqld.global.update_last_check_in_count++;
      return router_update_last_check_in_v2;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
