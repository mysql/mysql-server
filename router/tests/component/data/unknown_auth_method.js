// return a "switch-to: unknown_method" in the handshake.

({
  handshake: function(is_greeting) {
    if (is_greeting) {
      return {};
    } else {
      return {
        auth: {
          // switch to unknown.
          method_name: "unknown_method",
        }
      };
    }
  },
  stmts: function(stmt) {
    return {
      error: {
        code: 1023,
        message: "Unknown statement: " + stmt,
        sql_state: "HY000"
      }
    }
  }
})
