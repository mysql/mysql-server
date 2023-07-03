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

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
}

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.notices === undefined) {
  mysqld.global.notices = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.primary_id === undefined) {
  mysqld.global.primary_id = 0;
}

if (mysqld.global.mysqlx_wait_timeout_unsupported === undefined) {
  mysqld.global.mysqlx_wait_timeout_unsupported = 0;
}

if (mysqld.global.gr_notices_unsupported === undefined) {
  mysqld.global.gr_notices_unsupported = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
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
    nodes(mysqld.global.gr_node_host, mysqld.global.gr_nodes);

var options = {
  metadata_schema_version: [1, 0, 2],
  group_replication_membership: group_replication_membership_online,
  gr_id: mysqld.global.gr_id
};

var router_start_transaction =
    common_stmts.get("router_start_transaction", options);

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership.length === 0 ?
    "" :
    options.group_replication_membership[mysqld.global.primary_id][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "select_port", "router_commit", "router_select_schema_version",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
      "router_set_gr_consistency_level", "router_set_session_options"
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_update_attributes_v1",
    ],
    options);

var router_select_metadata =
    common_stmts.get("router_select_metadata", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else if (stmt === "set @@mysqlx_wait_timeout = 28800") {
      if (mysqld.global.mysqlx_wait_timeout_unsupported === 0) return {
          ok: {}
        }
      else
        return {
          error: {
            code: 1193,
            sql_state: "HY001",
            message: "Unknown system variable 'mysqlx_wait_timeout'"
          }
        }
    } else if (stmt === "enable_notices") {
      if (mysqld.global.gr_notices_unsupported === 0) return {
          ok: {}
        }
      else
        return {
          error: {
            code: 5163,
            sql_state: "HY001",
            message:
                "Invalid notice name group_replication/membership/quorum_loss"
          }
        }
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
