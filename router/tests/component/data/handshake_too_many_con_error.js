var common_stmts = require("common_statements");


({
  handshake: {
    error: {code: 1040, sql_state: "HY000", message: "Too many connections"}
  },
  stmts: function(stmt) {
    return common_stmts.unknown_statement_response(stmt);
  }
})
