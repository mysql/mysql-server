var common_stmts = require("common_statements");

var select_port = common_stmts.get("select_port");

({
  handshake: {
    auth: {
      username: "username",
      password: "password",
    }
  },
  stmts: function(stmt) {
    if (stmt === select_port.stmt) {
      return select_port;
    } else if (stmt === "SHOW STATUS LIKE 'Ssl_session_cache_hits'") {
      return {
        result: {
          columns: [{name: "Ssl_session_cache_hits", type: "LONG"}],
          rows: [[mysqld.session.ssl_session_cache_hits]]
        }
      }
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
