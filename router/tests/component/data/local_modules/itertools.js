var log = new Duktape.Logger()

exports.take =
    function(times, iter) {
  return new Duktape.Thread(function() {
    var yield = Duktape.Thread.yield;
    var resume = Duktape.Thread.resume;

    var rounds = 0;
    while (rounds++ < times) {
      var v = resume(iter);

      yield(v);
    }
  });
}

    exports.repeat =
        function(obj) {
  return new Duktape.Thread(function() {
    var yield = Duktape.Thread.yield;

    while (true) yield(obj);
  });
}

        // collect a iterable into a array
        exports.collect =
            function(iter) {
  var resume = Duktape.Thread.resume;

  var ar = [];

  while (true) {
    v = resume(iter);

    if (v === undefined) break;

    ar.push(v);
  }

  return ar;
}

            exports.chain =
                function(iters) {
  return new Duktape.Thread(function() {
    var yield = Duktape.Thread.yield;
    var resume = Duktape.Thread.resume;

    log.info(".chain()")

    // we can't use for-in here, as it doesn't operate on ndx-order
    // we can't use .forEach() is duktape doesn't allow yield in it
    for (var ndx = 0; ndx < iters.length; ndx++) {
      var iter = iters[ndx];
      while (true) {
        var elem = resume(iter);

        if (elem === undefined) break;

        yield(elem);
      }
    }
  });
}

                exports.cycle =
                    function(iter) {
  return new Duktape.Thread(function() {
    var yield = Duktape.Thread.yield;
    var resume = Duktape.Thread.resume;
    var buffer = [];

    while (true) {
      var elem = resume(iter);

      if (elem === undefined) break;

      yield(elem);
      buffer.push(elem);
    }

    if (buffer.length === 0) return;

    var ndx = 0;
    while (true) {
      if (ndx >= buffer.length) ndx = 0;
      log.info(".cycle()")
      yield(buffer[ndx++]);
    }
  });
}

                    // return an iterable for an array
                    exports.iter_arr = function(arr) {
  return new Duktape.Thread(function() {
    var yield = Duktape.Thread.yield;

    for (var ndx = 0; ndx < arr.length; ndx++) {
      yield(arr[ndx]);
    }
  });
}
