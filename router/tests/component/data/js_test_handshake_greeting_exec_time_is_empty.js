({
  // exec_time field is empty object, should be number
  //
  // invalid
  handshake: {greeting: {exec_time: {}}},
  stmts: function() {
    return {ok: {}};
  }

})
