define([
	'intern!object',
	'intern/chai!assert',
	'../../../Deferred'
], function (registerSuite, assert, Deferred) {
	// NOTE: At the time of this writing, Dojo promises can call resolve and reject handlers
	// on the same turn `then` is called, but these tests are written as if the handlers
	// are always called on the next turn so that they will not break if Dojo promises are made Promises/A+ compliant.
	// Any tests added to this suite should be written in this way.

	registerSuite({
		name: 'dojo/promise/Promise',

		'.always will be invoked for resolution and rejection': function () {
			var deferredToResolve = new Deferred();
			var expectedResolvedResult = {};
			var resolvedResult;
			var resolvedAlwaysResult;
			var deferredToReject = new Deferred();
			var expectedRejectedResult = {};
			var rejectedResult;
			var dfd = this.async();

			deferredToResolve.promise.then(function (result) {
				resolvedResult = result;
			});

			deferredToResolve.promise.always(function (result) {
				resolvedAlwaysResult = result;

				// Nest the rejected tests here to avoid chaining the promises under test
				deferredToReject.promise.then(null, function (result) {
					rejectedResult = result;
				});

				// Use this.async so we don't rely on the promise implementation under test
				deferredToReject.promise.always(dfd.callback(function (rejectedAlwaysResult) {
					assert.strictEqual(resolvedResult, expectedResolvedResult);
					assert.strictEqual(resolvedAlwaysResult, resolvedResult);
					assert.strictEqual(rejectedResult, expectedRejectedResult);
					assert.strictEqual(rejectedAlwaysResult, rejectedResult);
				}));

				deferredToReject.reject(expectedRejectedResult);
			});

			deferredToResolve.resolve(expectedResolvedResult);
		},

		'.otherwise() is equivalent to .then(null, ...)': function () {
			var deferred = new Deferred();
			var expectedResult = {};
			var rejectedResult;

			deferred.promise.then(null, function (result) {
				rejectedResult = result;
			});

			// Use this.async so we don't rely on the promise implementation under test
			deferred.promise.otherwise(this.async().callback(function (otherwiseResult) {
				assert.strictEqual(rejectedResult, expectedResult);
				assert.strictEqual(otherwiseResult, rejectedResult);
			}));

			deferred.reject(expectedResult);
		},

		'.trace() returns the same promise': function () {
			var deferred = new Deferred();
			var expectedPromise = deferred.promise;
			assert.strictEqual(expectedPromise.trace(), expectedPromise);
		},

		'.traceRejected() returns the same promise': function () {
			var deferred = new Deferred();
			var expectedPromise = deferred.promise;
			assert.strictEqual(expectedPromise.traceRejected(), expectedPromise);
		}
	});
});
