///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

var options = {
  metadata_schema_version: mysqld.global.metadata_version,
}

var router_select_schema_version =
  common_stmts.get("router_select_schema_version", options);

({
  stmts: function (stmt) {
    if (stmt === router_select_schema_version.stmt) {
      return router_select_schema_version;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
