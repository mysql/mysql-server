var common_stmts = require("common_statements");

var gr_node_host = "127.0.0.1";

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.view_id === undefined) {
  mysqld.global.view_id = 0;
}

if (mysqld.global.rest_user_credentials === undefined) {
  mysqld.global.rest_user_credentials = [];
}

if (mysqld.global.metadata_version === undefined) {
  mysqld.global.metadata_version = [0, 0, 0];
}

if (mysqld.global.rest_auth_query_count === undefined) {
  mysqld.global.rest_auth_query_count = 0;
}

if (mysqld.global.error_on_md_query === undefined) {
  mysqld.global.error_on_md_query = 0;
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
    nodes(gr_node_host, mysqld.global.gr_nodes, mysqld.global.gr_id);

var options = {
  metadata_schema_version: mysqld.global.metadata_version,
  group_replication_membership: group_replication_membership_online,
  gr_id: mysqld.global.gr_id,
  view_id: mysqld.global.view_id,
  primary_port:
      group_replication_membership_online[mysqld.global.primary_id][2],
  cluster_type: "gr",
  innodb_cluster_name: "test",
  rest_user_credentials: mysqld.global.rest_user_credentials
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership[mysqld.global.primary_id][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "select_port",
      "router_commit",
      "router_rollback",
      "router_select_metadata",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
      "router_select_metadata_v2_gr",
      "router_update_last_check_in_v2",
    ],
    options);

var router_select_rest_accounts_credentials =
    common_stmts.get("router_select_rest_accounts_credentials", options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_update_attributes_v2",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_select_rest_accounts_credentials.stmt) {
      mysqld.global.rest_auth_query_count++;
      if (mysqld.global.error_on_md_query === 1) {
        return {
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        }
      } else
        return router_select_rest_accounts_credentials;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
