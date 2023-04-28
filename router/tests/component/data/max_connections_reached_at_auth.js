var common_stmts = require("common_statements");

if (mysqld.global.connects === undefined) {
  mysqld.global.connects = 0;
}

mysqld.global.connects = mysqld.global.connects + 1;

({
  handshake: function(is_greeting) {
    if (!is_greeting && mysqld.global.connects == 1) {
      return {
        error: {code: 1040, sql_state: "HY000", message: "Too many connections"}
      };
    }

    return {};
  },
  stmts: function(stmt) {
    return common_stmts.unknown_statement_response(stmt);
  }
})
