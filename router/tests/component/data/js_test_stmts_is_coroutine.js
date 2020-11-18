// ensure that 'stmts' can be an 'thread'

function result_generator() {
  return new Duktape.Thread(function(stmt) {
    var yield = Duktape.Thread.yield;

    while (true) {
      // return ok for every statement received
      stmt = yield({ok: {}});
    }
  });
}

({stmts: result_generator()})
