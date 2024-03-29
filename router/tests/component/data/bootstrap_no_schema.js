///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
    ],
    {});

var router_select_schema_version =
    common_stmts.get("router_select_schema_version", {});

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_schema_version.stmt) {
      return {
        error: {
          code: 1049,
          sql_state: "HY001",
          message: "Unknown database 'mysql_innodb_cluster_metadata'"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
