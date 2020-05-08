define([
	'intern!object',
	'intern/chai!assert',
	'../../Deferred',
	'../../promise/Promise',
	'../../when'
], function (registerSuite, assert, Deferred, Promise, when) {
	var emptyObject = {};
	var dfd;

	registerSuite({
		name: 'dojo/when',

		'return the same promise without callbacks': function () {
			var promise1 = when(emptyObject);

			assert.instanceOf(promise1, Promise);

			dfd = new Deferred();
			var promise2 = when(dfd.promise);

			assert.instanceOf(promise2, Promise);
			assert.strictEqual(dfd.promise, promise2);
		},

		'do not convert to promise if errback is passed but no callback': function () {
			var result = when(emptyObject, null, function () {});

			assert.strictEqual(result, emptyObject);
		},

		'with a result value': function () {
			var received;

			when(emptyObject, function (result) {
				received = result;
			});
			assert.strictEqual(received, emptyObject);
		},

		'with a result value, return result of callback': function () {
			var obj1 = {};
			var obj2 = {};
			var received;
			var returned = when(obj1, function (result) {
				received = result;
				return obj2;
			});

			assert.strictEqual(received, obj1);
			assert.strictEqual(returned, obj2);
		},

		'with a promise that gets resolved': function () {
			var received;

			dfd = new Deferred();
			when(dfd.promise, function (result) {
				received = result;
			});
			dfd.resolve(emptyObject);
			assert.strictEqual(received, emptyObject);
		},

		'with a promise that gets rejected': function () {
			var received;

			dfd = new Deferred();
			when(dfd.promise, null, function (result) {
				received = result;
			});
			dfd.reject(emptyObject);
			assert.strictEqual(received, emptyObject);
		},

		'with a promise that gets progress': function () {
			var received;

			dfd = new Deferred();
			when(dfd.promise, null, null, function (result) {
				received = result;
			});
			dfd.progress(emptyObject);
			assert.strictEqual(received, emptyObject);
		},

		'with chaining of the result': function () {
			var received;

			function square(n){ return n * n; }

			when(2).then(square).then(square).then(function (n) {
				received = n;
			});
			assert.strictEqual(received, 16);
		},

		'convert foreign promise': function () {
			var _callback;
			var foreign = {
				then: function (cb) {
					_callback = cb;
				}
			};
			var promise = when(foreign);
			var received;

			promise.then(function (result) {
				received = result;
			});
			_callback(emptyObject);
			assert.instanceOf(promise, Promise);
			assert.strictEqual(received, emptyObject);
		}
	});
});
