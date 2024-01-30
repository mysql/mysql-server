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
  mysqld.global.gr_id = "uuid";
}

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.view_id === undefined) {
  mysqld.global.view_id = 0;
}

if (mysqld.global.error_on_md_query === undefined) {
  mysqld.global.error_on_md_query = 0;
}

if (mysqld.global.cluster_name === undefined) {
  mysqld.global.cluster_name = "test";
}

if (mysqld.global.cluster_type === undefined) {
  mysqld.global.cluster_type = "ar";
}

if (mysqld.global.router_options === undefined) {
  mysqld.global.router_options = "";
}

if (mysqld.global.update_last_check_in_count === undefined) {
  mysqld.global.update_last_check_in_count = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
}

if (mysqld.global.metadata_schema_version === undefined) {
  mysqld.global.metadata_schema_version = [2, 2, 0];
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0], host, current_value[1], current_value[2],
      current_value[3]
    ];
  });
};

var cluster_nodes = gr_memberships.cluster_nodes(
    mysqld.global.gr_node_host, mysqld.global.cluster_nodes)

var options = {
  innodb_cluster_instances: cluster_nodes,
  cluster_id: mysqld.global.gr_id,
  view_id: mysqld.global.view_id,
  cluster_type: mysqld.global.cluster_type,
  innodb_cluster_name: mysqld.global.cluster_name,
  router_options: mysqld.global.router_options,
  metadata_schema_version: mysqld.global.metadata_schema_version,
};

var select_port = common_stmts.get("select_port", options);

var router_set_session_options =
    common_stmts.get("router_set_session_options", options);

var router_set_gr_consistency_level =
    common_stmts.get("router_set_gr_consistency_level", options);

var router_update_attributes =
    common_stmts.get("router_update_attributes_v2", options);

var router_update_last_check_in_v2 =
    common_stmts.get("router_update_last_check_in_v2", options);

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_commit",
      "router_rollback",
      "router_select_cluster_type_v2",
      "router_select_schema_version",
      "router_select_view_id_v2_ar",
      "router_select_router_options_view",
    ],
    options);

var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_ar", options);

var router_start_transaction =
    common_stmts.get("router_start_transaction", options);

var router_select_cluster_type =
    common_stmts.get("router_select_cluster_type_v2", options);

({
  handshake: {
    auth: {
      username: mysqld.global.user,
      password: mysqld.global.password,
    }
  },
  stmts: function(stmt) {
    if (stmt === select_port.stmt) {
      return select_port;
    }
    if (stmt === router_set_session_options.stmt) {
      return router_set_session_options;
    }
    if (stmt === router_set_gr_consistency_level.stmt) {
      return router_set_gr_consistency_level;
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (mysqld.global.error_on_md_query === 1) {
      mysqld.global.md_query_count++;
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      }
    } else if (stmt === router_update_last_check_in_v2.stmt) {
      mysqld.global.update_last_check_in_count++;
      return router_update_last_check_in_v2;
    } else if (res = stmt.match(router_update_attributes.stmt_regex)) {
      mysqld.global.upd_attr_config_json = res[7];

      mysqld.global.update_attributes_count++;
      return router_update_attributes;
    } else if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
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
