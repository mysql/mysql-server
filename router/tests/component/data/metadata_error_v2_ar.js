var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if(mysqld.global.transaction_count === undefined){
    mysqld.global.transaction_count = 0;
}

({
  stmts: function (stmt) {
    var options = {
      cluster_id: mysqld.global.gr_id,
      view_id: mysqld.global.view_id,
      cluster_type: "ar",
    };

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses([
      "select_port",
    ], options);

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else {
      mysqld.global.transaction_count++;
      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      }
    }
  }
})
