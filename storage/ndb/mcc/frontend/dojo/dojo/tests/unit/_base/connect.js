define([
	'intern!object',
	'intern/chai!assert',
	'testing'
], function (
	registerSuite,
	assert,
	dojo
) {
	// make 'iterations' connections to hub
	// roughly half of which will be to 'good' and
	// half to 'bad'
	// all connections to 'bad' are disconnected
	// test can then be performed on the values
	// 'failures' and 'successes'
	function markAndSweepTest(iterations) {
		var hub = function () {};
		var failures = 0;
		var good = function () {};
		var bad = function () {
			failures++;
		};
		var marked = [];
		var m;

		// connections
		for (var i = 0; i < iterations; i++) {
			if (Math.random() < 0.5) {
				marked.push(dojo.connect('hub', bad));
			} else {
				dojo.connect('hub', good);
			}
		}

		// Randomize markers (only if the count isn't very high)
		if (i < Math.pow(10, 4)) {
			var rm = [ ];
			while (marked.length) {
				m = Math.floor(Math.random() * marked.length);
				rm.push(marked[m]);
				marked.splice(m, 1);
			}
			marked = rm;
		}

		for (m = 0; m < marked.length; m++) {
			dojo.disconnect(marked[m]);
		}

		// test
		failures = 0;
		hub();

		// return number of disconnected functions that fired (should be 0)
		return failures;
	}

	function markAndSweepSubscribersTest(iterations) {
		var failures = 0;
		var good = function () {};
		var bad = function () {
			failures++;
		};
		var topic = 'hubbins';
		var marked = [];
		var m;

		// connections
		for (var i = 0; i < iterations; i++) {
			if (Math.random() < 0.5) {
				marked.push(dojo.subscribe(topic, bad));
			} else {
				dojo.subscribe(topic, good);
			}
		}

		// Randomize markers (only if the count isn't very high)
		if (i < Math.pow(10, 4)) {
			var rm = [];
			while (marked.length) {
				m = Math.floor(Math.random() * marked.length);
				rm.push(marked[m]);
				marked.splice(m, 1);
			}
			marked = rm;
		}

		for (m = 0; m < marked.length; m++) {
			dojo.unsubscribe(marked[m]);
		}

		// test
		failures = 0;
		dojo.publish(topic);
		// return number of unsubscribed functions that fired (should be 0)
		return failures;
	}

	registerSuite({
		name: 'dojo/_base/connect',

		afterEach: function () {
			dojo.global.gFoo = undefined;
			dojo.global.gOk = undefined;
		},

		'smoke test': function () {
			var ok;
			var test = { 'foo': function () { ok = false; } };

			test.foo();
			assert.isFalse(ok);

			dojo.connect(test, 'foo', null, function () { ok = true; });

			test.foo();
			assert.isTrue(ok);
		},

		'basic test': function () {
			var out = '';
			var obj = {
				foo: function () {
					out += 'foo';
				},
				bar: function () {
					out += 'bar';
				},
				baz: function () {
					out += 'baz';
				}
			};

			var handle = dojo.connect(obj, 'foo', obj, 'bar');
			dojo.connect(obj, 'bar', obj, 'baz');

			out = '';
			obj.foo();
			assert.equal(out, 'foobarbaz');

			out = '';
			obj.bar();
			assert.equal(out, 'barbaz');

			out = '';
			obj.baz();
			assert.equal(out, 'baz');

			dojo.connect(obj, 'foo', obj, 'baz');
			dojo.disconnect(handle);

			out = '';
			obj.foo();
			assert.equal(out, 'foobaz');

			out = '';
			obj.bar();
			assert.equal(out, 'barbaz');

			out = '';
			obj.baz();
			assert.equal(out, 'baz');
		},

		'hub connect disconnect 1000': function () {
			assert.equal(markAndSweepTest(1000), 0);
		},

		'test with four arguments': function () {
			var ok;
			var obj = {
				foo: function () {
					ok = false;
				},
				bar: function () {
					ok = true;
				}
			};

			dojo.connect(obj, 'foo', obj, 'bar');
			obj.foo();
			assert.isTrue(ok);
		},

		'test with three arguments': function () {
			var ok;
			var link;

			dojo.global.gFoo = function () {
				ok = false;
			};
			dojo.global.gOk = function () {
				ok = true;
			};

			// 3 arg shorthand for globals (a)
			link = dojo.connect('gFoo', null, 'gOk');
			dojo.global.gFoo();
			dojo.disconnect(link);
			assert.isTrue(ok);

			// 3 arg shorthand for globals (b)
			link = dojo.connect(null, 'gFoo', 'gOk');
			dojo.global.gFoo();
			dojo.disconnect(link);
			assert.isTrue(ok);

			// verify disconnections
			dojo.global.gFoo();
			assert.isFalse(ok);
		},

		'test with two arguments': function () {
			var ok;
			var link;

			dojo.global.gFoo = function () {
				ok = false;
			};
			dojo.global.gOk = function () {
				ok = true;
			};

			// 2 arg shorthand for globals
			link = dojo.connect('gFoo', 'gOk');
			dojo.global.gFoo();
			dojo.disconnect(link);
			assert.isTrue(ok);

			// 2 arg shorthand for globals, alternate scoping
			link = dojo.connect('gFoo', dojo.global.gOk);
			dojo.global.gFoo();
			dojo.disconnect(link);
			assert.isTrue(ok);
		},

		'scope test one': function () {
			var foo = {
				ok: true,
				foo: function () {
					this.ok = false;
				}
			};
			var bar = {
				ok: false,
				bar: function () {
					this.ok = true;
				}
			};

			// link foo.foo to bar.bar with natural scope
			dojo.connect(foo, 'foo', bar, 'bar');
			foo.foo();
			assert.isFalse(foo.ok);
			assert.isTrue(bar.ok);
		},

		'scope test two': function () {
			var foo = {
				ok: true,
				foo: function () {
					this.ok = false;
				}
			};
			var bar = {
				ok: false,
				bar: function () {
					this.ok = true;
				}
			};

			// link foo.foo to bar.bar such that scope is always 'foo'
			dojo.connect(foo, 'foo', bar.bar);
			foo.foo();
			assert.isTrue(foo.ok);
			assert.isFalse(bar.ok);
		},

		'pubsub': function () {
			var count = 0;
			dojo.subscribe('/test/blah', function (first, second) {
				assert.equal(first, 'first');
				assert.equal(second, 'second');
				count++;
			});
			dojo.publish('/test/blah', ['first', 'second']);
			assert.equal(count, 1);
		},

		'connect publisher': function () {
			var foo = {
				inc: 0,
				foo: function () {
					this.inc++;
				}
			};
			var bar = {
				inc: 0,
				bar: function () {
					this.inc++;
				}
			};
			var c1h = dojo.connectPublisher('/blah', foo, 'foo');
			var c2h = dojo.connectPublisher('/blah', foo, 'foo');

			dojo.subscribe('/blah', bar, 'bar');
			foo.foo();
			assert.equal(foo.inc, 1);
			assert.equal(bar.inc, 2);

			dojo.disconnect(c1h);
			foo.foo();
			assert.equal(foo.inc, 2);
			assert.equal(bar.inc, 3);

			dojo.disconnect(c2h);
			foo.foo();
			assert.equal(foo.inc, 3);
			assert.equal(bar.inc, 3);
		},

		'publish subscribe 1000': function () {
			assert.equal(markAndSweepSubscribersTest(1000), 0);
		}
	});
});
