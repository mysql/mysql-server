var common_stmts = require("common_statements");
var select_port = common_stmts.get("select_port");

({
  stmts: function (stmt) {
    if (stmt === select_port.stmt) {
      return select_port;
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
