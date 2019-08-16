var common_stmts = require("common_statements");
var select_port = common_stmts.get("select_port");

// allow to change the connect_exec_time before the greeting of the server is sent
var connect_exec_time = (mysqld.global.connect_exec_time === undefined)
  ? 0 : mysqld.global.connect_exec_time;

({
  handshake: {
    greeting: {
      exec_time: connect_exec_time
    },
    auth: {
      username: mysqld.global.username,
      password: mysqld.global.password
    }
  },
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
