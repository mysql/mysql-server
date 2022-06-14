
({
  stmts: function(stmt) {
    return {
      error: {
        code: 1234,
        sql_state: "HY001",
        message: "Some unexpected error occured"
      }
    }
  }
})
