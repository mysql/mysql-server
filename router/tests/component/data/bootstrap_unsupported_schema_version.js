///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

if (mysqld.global.metadata_schema_version === undefined) {
  mysqld.global.metadata_schema_version = [0, 1, 0]
}

var options = {
  metadata_schema_version: mysqld.global.metadata_schema_version,
}

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
    ],
    options);

var router_select_schema_version =
    common_stmts.get("router_select_schema_version", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_schema_version.stmt) {
      return router_select_schema_version;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
