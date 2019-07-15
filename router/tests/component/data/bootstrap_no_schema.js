///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

var router_select_schema_version =
  common_stmts.get("router_select_schema_version", {});

({
  stmts: function (stmt) {
    if (stmt === router_select_schema_version.stmt) {
      return {
        error: {
          code: 1049,
          sql_state: "HY001",
          message: "Unknown database 'mysql_innodb_cluster_metadata'"
        }
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
