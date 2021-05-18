var statements = require("common_statements");

/**
 *
 */
function my_query_generator() {
  return new Duktape.Thread(function(stmt) {
    var yield = Duktape.Thread.yield;
    var mysql_client_select_user = statements.get("mysql_client_select_user");
    var mysql_client_select_version_comment =
        statements.get("mysql_client_select_version_comment");
    var select_port = statements.get("select_port");

    while (true) {
      if (stmt === mysql_client_select_user.stmt) {
        stmt = yield(mysql_client_select_user);
      } else if (stmt === mysql_client_select_version_comment.stmt) {
        stmt = yield(mysql_client_select_version_comment);
      } else if (stmt === select_port.stmt) {
        stmt = yield(select_port);
      } else {
        stmt = yield({
          error: {
            code: 1273,
            sql_state: "HY001",
            message: "Syntax Error at: " + stmt
          }
        });
      }
    }
  });
}

({stmts: my_query_generator()})
