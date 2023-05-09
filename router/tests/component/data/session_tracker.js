({
  stmts: function(stmt) {
    if (stmt === "USE some_schema") {
      return {
        ok: {
          session_trackers: [{
            type: "schema",
            schema: "some_schema",
          }]
        }
      };
    } else if (stmt === "SET sysvar1=1, sysvar2=2") {
      return {
        ok: {
          session_trackers: [
            {
              type: "system_variable",
              name: "sysvar_a",
              value: "1",
            },
            {
              type: "system_variable",
              name: "sysvar_b",
              value: "2",
            }
          ]
        }
      };
    } else if (stmt === "INSERT ...") {
      return {
        ok: {
          session_trackers: [
            {
              type: "gtid",
              gtid: "3E11FA47-71CA-11E1-9E33-C80AA9429562:1",
            },
          ]
        }
      };
    } else if (stmt === "INSERT @user_var") {
      return {
        ok: {
          session_trackers: [
            {
              type: "gtid",
              gtid: "3E11FA47-71CA-11E1-9E33-C80AA9429562:2",
            },
            {
              type: "state",
              state: "1",
            },
            {
              type: "trx_state",
              state: "________",
            },
            {
              type: "trx_characteristics",
              trx_stmt: "",
            },
          ]
        }
      };
    } else {
      console.log(stmt);

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
