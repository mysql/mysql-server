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

var gr_node_host = "127.0.0.1";

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "uuid";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.view_id === undefined) {
  mysqld.global.view_id = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
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

if (mysqld.global.router_rw_split_classic_port === undefined) {
  mysqld.global.router_rw_split_classic_port = "";
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

if (mysqld.global.upd_attr_router_version === undefined) {
  mysqld.global.upd_attr_router_version = "";
}

if (mysqld.global.upd_attr_rw_classic_port === undefined) {
  mysqld.global.upd_attr_rw_classic_port = "";
}

if (mysqld.global.upd_attr_ro_classic_port === undefined) {
  mysqld.global.upd_attr_rw_classic_port = "";
}

if (mysqld.global.upd_attr_rw_x_port === undefined) {
  mysqld.global.upd_attr_rw_x_port = "";
}

if (mysqld.global.upd_attr_ro_x_port === undefined) {
  mysqld.global.upd_attr_ro_x_port = "";
}

if (mysqld.global.upd_attr_md_username === undefined) {
  mysqld.global.upd_attr_md_username = "";
}

if (mysqld.global.upd_attr_config_json === undefined) {
  mysqld.global.upd_attr_config_json = "";
}

if (mysqld.global.upd_attr_config_update_schema_json === undefined) {
  mysqld.global.upd_attr_config_update_schema_json = "";
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0],
      host,
      current_value[1],
      current_value[2],
    ];
  });
};

var cluster_members_online =
    nodes(gr_node_host, mysqld.global.gr_nodes, mysqld.global.gr_id);

var metadata_version =
    (mysqld.global.upgrade_in_progress === 1) ? [0, 0, 0] : [2, 0, 0];
var options = {
  metadata_schema_version: metadata_version,
  innodb_cluster_instances: cluster_members_online,
  cluster_id: mysqld.global.gr_id,
  view_id: mysqld.global.view_id,
  cluster_type: "ar",
  innodb_cluster_name: "test",
  router_version: mysqld.global.router_version,
  router_rw_classic_port: mysqld.global.router_rw_classic_port,
  router_ro_classic_port: mysqld.global.router_ro_classic_port,
  router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
  router_rw_x_port: mysqld.global.router_rw_x_port,
  router_ro_x_port: mysqld.global.router_ro_x_port,
  router_metadata_user: mysqld.global.router_metadata_user,
};

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options", "router_set_gr_consistency_level",
      "router_commit", "router_rollback", "router_select_schema_version",
      "router_select_cluster_type_v2", "router_select_view_id_v2_ar",
      "router_update_last_check_in_v2", "select_port"
    ],
    options);


var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_ar", options);

var router_update_attributes =
    common_stmts.get("router_update_attributes_v2", options);

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
    } else if (res = stmt.match(router_update_attributes.stmt_regex)) {
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
      } else {
        mysqld.global.upd_attr_router_version = res[1];
        mysqld.global.upd_attr_rw_classic_port = res[2];
        mysqld.global.upd_attr_ro_classic_port = res[3];
        mysqld.global.upd_attr_rw_split_classic_port = res[4];
        mysqld.global.upd_attr_rw_x_port = res[5];
        mysqld.global.upd_attr_ro_x_port = res[6];
        mysqld.global.upd_attr_md_username = res[7];
        mysqld.global.upd_attr_config_json = res[8];
        return router_update_attributes;
      }
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
