define([
	'intern!object',
	'intern/chai!assert',
	'dojo/Deferred',
	'../../../promise/all'
], function (registerSuite, assert, Deferred, all) {
	registerSuite({
		name: 'dojo/promise/all',

		'array argument': function () {
			var expectedResults = [
					'foo',
					'bar',
					'baz'
				],
				deferreds = [
					new Deferred(),
					new Deferred(),
					expectedResults[2]
				];

			deferreds[1].resolve(expectedResults[1]);
			all(deferreds).then(this.async().callback(function (results) {
				assert.deepEqual(results, expectedResults);
			}));
			deferreds[0].resolve(expectedResults[0]);
		},

		'object argument': function () {
			var expectedResultHash = {
					a: 'foo',
					b: 'bar',
					c: 'baz'
				},
				deferredHash = {
					a: new Deferred(),
					b: new Deferred(),
					c: expectedResultHash.c
				};

			deferredHash.a.resolve(expectedResultHash.a);
			all(deferredHash).then(this.async().callback(function (resultHash) {
				assert.deepEqual(resultHash, expectedResultHash);
			}));
			deferredHash.b.resolve(expectedResultHash.b);
		},

		'without arguments': function () {
			all().then(this.async().callback(function (result) {
				assert.isTrue(typeof result === 'undefined');
			}));
		},

		'with single non-object argument': function () {
			all(null).then(this.async().callback(function (result) {
				assert.isTrue(typeof result === 'undefined');
			}));
		},

		'with empty array': function () {
			all([]).then(this.async().callback(function (result) {
				assert.deepEqual(result, []);
			}));
		},

		'with empty object': function () {
			all({}).then(this.async().callback(function (result) {
				assert.deepEqual(result, {});
			}));
		},

		'with one rejected promise': function () {
			var expectedRejectedResult = {};
			var argument = [ new Deferred(), new Deferred(), {} ];

			argument[1].reject(expectedRejectedResult);
			all(argument).then(null, this.async().callback(function (result) {
				assert.strictEqual(result, expectedRejectedResult);
			}));
		},

		'with one promise rejected later': function () {
			var expectedRejectedResult = {};
			var argument = [ new Deferred(), new Deferred(), {} ];

			all(argument).then(null, this.async().callback(function (result) {
				assert.strictEqual(result, expectedRejectedResult);
			}));
			argument[1].reject(expectedRejectedResult);
		},

		'with multiple promises rejected later': function () {
			var expectedRejectedResult = {};
			var actualResult;
			var argument = [ new Deferred(), new Deferred(), {} ];

			all(argument).then(null, function (result) {
				actualResult = result;
			});

			argument[0].reject(expectedRejectedResult);
			argument[1].reject({});

			// ensure reject is only called once
			setTimeout(this.async().callback(function () {
				assert.strictEqual(actualResult, expectedRejectedResult);
			}), 0);
		},

		'cancel only affects returned promise, not those we\'re waiting for': function () {
			var expectedCancelResult = {};
			var deferredCanceled = false;
			var secondDeferred = new Deferred(function () { deferredCanceled = true; });

			all([ new Deferred(), secondDeferred, new Deferred() ]).then(null, this.async().callback(function (result) {
				assert.strictEqual(result, expectedCancelResult);
				assert.isFalse(deferredCanceled);
			})).cancel(expectedCancelResult);
		}
	});
});
