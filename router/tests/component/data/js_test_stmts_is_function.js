// test that 'stmts' can be an function

({
  stmts: function(stmt) {
    // return ok for every statement received
    return {
      ok: {}
    }
  }
})
