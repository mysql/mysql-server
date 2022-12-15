var common_stmts = require("common_statements");
// var select_port = common_stmts.get("select_port");
// var start_transaction = common_stmts.get("router_start_transaction");

// allow to change the connect_exec_time before the greeting of the server is
// sent
var connect_exec_time = (mysqld.global.connect_exec_time === undefined) ?
    0 :
    mysqld.global.connect_exec_time;

var options = {
  //  group_replication_members: group_replication_members_online,
  cluster_type: "gr",
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options", "router_set_gr_consistency_level",
      "select_port", "router_start_transaction", "router_commit",
      "router_select_schema_version", "router_select_cluster_type_v2",
      "router_select_metadata_v2_gr",
      //  "router_select_group_membership_with_primary_mode",
    ],
    options);

({
  handshake: {
    greeting: {exec_time: connect_exec_time},
    auth: {username: mysqld.global.username, password: mysqld.global.password}
  },
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else {
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      };
    }
  }
})
