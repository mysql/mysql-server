/**
 * schema got created, but group replication isn't started.
 */

///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

var common_responses = common_stmts.prepare_statement_responses([
  "router_select_schema_version",
], {});

var router_count_clusters_and_replicasets =
  common_stmts.get("router_count_clusters_and_replicasets", {});

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if (stmt === router_count_clusters_and_replicasets.stmt) {
      return {
        error: {
          code: 1193,
          sql_state: "HY001",
          message: "Unknown system variable 'group_replication_group_name'"
        }
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
