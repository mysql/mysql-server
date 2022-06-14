var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.view_id === undefined) {
  mysqld.global.view_id = 0;
}

var options = {
  cluster_id: mysqld.global.gr_id,
  view_id: mysqld.global.view_id,
  cluster_type: "ar",
};

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_view_id_v2_ar",
      "select_port",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
