var common_stmts = require("common_statements");

var options = {};

var common_responses = common_stmts.prepare_callable_statement_responses(
    [
      "select_length_4097",
      "select_repeat_4097",
      "select_repeat_15M",
      "mysqlsh_select_connection_id",
      "mysqlsh_select_version_comment",
      "router_show_cipher_status",
      "router_show_mysqlx_cipher_status",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt]();
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
