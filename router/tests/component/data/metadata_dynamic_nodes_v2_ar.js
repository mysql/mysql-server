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

if(mysqld.global.gr_node_host === undefined){
    mysqld.global.gr_node_host = "127.0.0.1";
}

if(mysqld.global.gr_id === undefined){
    mysqld.global.gr_id = "00-000";
}

if(mysqld.global.gr_nodes === undefined){
    mysqld.global.gr_nodes = [];
}

if(mysqld.global.md_query_count === undefined){
    mysqld.global.md_query_count = 0;
}

if(mysqld.global.transaction_count === undefined){
    mysqld.global.transaction_count = 0;
}

if(mysqld.global.primary_id === undefined){
    mysqld.global.primary_id = 0;
}

if(mysqld.global.view_id === undefined){
    mysqld.global.view_id = 0;
}

if(mysqld.global.error_on_md_query === undefined){
    mysqld.global.error_on_md_query = 0;
}

if(mysqld.global.empty_result_from_cluster_type_query === undefined){
    mysqld.global.empty_result_from_cluster_type_query = 0;
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function (current_value) {
    return [ current_value[0], host, current_value[0], current_value[1], current_value[2]];
  });
};

({
  stmts: function (stmt) {
    var group_replication_membership_online =
      nodes(mysqld.global.gr_node_host, mysqld.global.gr_nodes, mysqld.global.gr_id);

    var options = {
      group_replication_membership: group_replication_membership_online,
      cluster_id: mysqld.global.gr_id,
      view_id: mysqld.global.view_id,
      primary_port: group_replication_membership_online[mysqld.global.primary_id][2],
      cluster_type: "ar",
      innodb_cluster_name: "test",
    };

    // first node is PRIMARY
    options.group_replication_primary_member = options.group_replication_membership[mysqld.global.primary_id][0];

    var select_port =
        common_stmts.get("select_port", options);

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses([
      "router_commit",
      "router_rollback",
      "router_select_schema_version",
      "router_select_view_id_v2_ar",
      "router_update_last_check_in_v2",
    ], options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex([
      "router_update_version_v2",
    ], options);

    var router_select_metadata =
        common_stmts.get("router_select_metadata_v2_ar", options);

    var router_start_transaction =
        common_stmts.get("router_start_transaction", options);

    var router_select_cluster_type =
        common_stmts.get("router_select_cluster_type_v2", options);

    if (stmt === select_port.stmt) {
      return select_port;
    }
    else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    }
    else if (mysqld.global.error_on_md_query === 1) {
      mysqld.global.md_query_count++;
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      }
    }
    else if (stmt === router_select_cluster_type.stmt) {
      if (mysqld.global.empty_result_from_cluster_type_query === 1)
        return { "result": {
                  "columns": [
                    {
                      "type": "STRING",
                      "name": "cluster_type"
                    }
                   ],
                   "rows": []
                }}
      else
        return router_select_cluster_type;
    }
    else if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
      return mysqld.global.notices;
  })()
})
