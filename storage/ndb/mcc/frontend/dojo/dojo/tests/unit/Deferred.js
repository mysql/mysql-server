define([
	'intern!object',
	'intern/chai!assert',
	'sinon',
	'../../Deferred',
	'../../promise/Promise',
	'../../errors/CancelError'
], function (registerSuite, assert, sinon, Deferred, Promise, CancelError) {
	registerSuite(function () {
		var canceler;
		var deferred;

		return {
			name: 'dojo/Deferred',

			'beforeEach': function () {
				canceler = sinon.stub();
				deferred = new Deferred(canceler);
			},

			'.resolve': {
				'deferred receives result after resolving': function () {
					var expected = {};

					deferred.resolve(expected);
					return deferred.then(function (result) {
						assert.equal(result, expected);
					});
				},

				'promise receives result after resolving': function () {
					var expected = {};

					deferred.resolve(expected);
					return deferred.promise.then(function (result) {
						assert.equal(result, expected);
					});
				},

				'resolve() returns promise': function () {
					var obj = {};
					var returnedPromise = deferred.resolve(obj);

					assert.instanceOf(returnedPromise, Promise);
					assert.equal(returnedPromise, deferred.promise);
				},

				'isResolved() returns true after resolving': function () {
					assert.isFalse(deferred.isResolved());
					deferred.resolve();
					assert.isTrue(deferred.isResolved());
				},

				'isFulfilled() returns true after resolving': function () {
					assert.isFalse(deferred.isFulfilled());
					deferred.resolve();
					assert.isTrue(deferred.isFulfilled());
				},

				'resolve() is ignored after having been fulfilled': function () {
					assert.doesNotThrow(function () {
						deferred.resolve();
						deferred.resolve();
					});
				},

				'resolve() throws error after having been fulfilled and strict': function () {
					deferred.resolve();
					assert.throws(function () {
						deferred.resolve({}, true);
					}, Error);
				},

				'resolve() watching success handlers are notified': function () {
					var expected = {};
					var successHandler = sinon.spy(function (result) {
						assert.equal(result, expected);
					});

					deferred.then(successHandler);
					assert.isFalse(successHandler.called);
					deferred.resolve(expected);
					assert.isTrue(successHandler.called);
				},

				'resolve() is already bound to the deferred': function () {
					var expected = {};
					var resolve = deferred.resolve;

					resolve(expected);
					return deferred.then(function (result) {
						assert.equal(result, expected);
					});
				}
			},

			'.reject': {
				'deferred receives result after rejecting': function () {
					var expected = {};

					deferred.reject(expected);
					return deferred.then(null, function (result) {
						assert.equal(result, expected);
					});
				},

				'promise receives result after rejecting': function () {
					var expected = {};

					deferred.reject(expected);
					return deferred.promise.then(null, function (result) {
						assert.equal(result, expected);

					});
				},

				'reject() returns promise': function () {
					var returnedPromise = deferred.reject({});

					assert.instanceOf(returnedPromise, Promise);
					assert.equal(returnedPromise, deferred.promise);
				},

				'isRejected() returns true after rejecting': function () {
					assert.isFalse(deferred.isRejected());
					deferred.reject();
					assert.isTrue(deferred.isRejected());
				},

				'isFulfilled() returns true after rejecting': function () {
					assert.isFalse(deferred.isFulfilled());
					deferred.reject();
					assert.isTrue(deferred.isFulfilled());
				},

				'reject() is ignored after having been fulfilled': function () {
					assert.doesNotThrow(function () {
						deferred.reject();
						deferred.reject();
					});
				},

				'reject() throws error after having been fulfilled and strict': function () {
					deferred.reject();
					assert.throws(function () {
						deferred.reject({}, true);
					}, Error);
				},

				'reject() watching failure handlers are notified': function () {
					var expected = {};
					var failureHandler = sinon.spy(function (result) {
						assert.equal(result, expected);
					});

					deferred.then(null, failureHandler);
					assert.isFalse(failureHandler.called);
					deferred.reject(expected);
					assert.isTrue(failureHandler.called);
				},

				'reject() is already bound to the deferred': function () {
					var expected = {};
					var reject = deferred.reject;

					reject(expected);
					return deferred.then(null, function (result) {
						assert.equal(result, expected);
					});
				}
			},
			'.progress': {
				'deferred receives result after progress': function () {
					var expected = {};
					var progressStub = sinon.stub();

					deferred.then(null, null, progressStub);
					deferred.progress(expected);
					assert.isTrue(progressStub.calledOnce);
					assert.equal(progressStub.lastCall.args[0], expected);
				},

				'promise receives result after progres': function () {
					var expected = {};
					var progressStub = sinon.stub();

					deferred.promise.then(null, null, progressStub);
					deferred.progress(expected);
					assert.isTrue(progressStub.calledOnce);
					assert.equal(progressStub.lastCall.args[0], expected);
				},

				'progress() returns promise': function () {
					var returnedPromise = deferred.progress({});

					assert.instanceOf(returnedPromise, Promise);
					assert.equal(returnedPromise, deferred.promise);
				},

				'isResolved() returns false after progress': function () {
					assert.isFalse(deferred.isResolved());
					deferred.progress();
					assert.isFalse(deferred.isResolved());
				},

				'isRejected() returns false after progress': function () {
					assert.isFalse(deferred.isRejected());
					deferred.progress();
					assert.isFalse(deferred.isRejected());
				},

				'isFulfilled() returns false after progress': function () {
					assert.isFalse(deferred.isFulfilled());
					deferred.progress();
					assert.isFalse(deferred.isFulfilled());
				},

				'progress() is ignored after having been fulfilled': function () {
					var progressStub = sinon.stub();

					deferred.promise.then(null, null, progressStub);
					deferred.resolve();
					deferred.progress();
					assert.isFalse(progressStub.called);
				},

				'progress() throws error after having been fulfilled and strict': function () {
					deferred.resolve();
					assert.throws(function () {
						deferred.progress({}, true);
					}, Error);
				},

				'progress() results are not cached': function () {
					var firstProgressData = {};
					var secondProgressData = {};
					var progressStub = sinon.stub();

					deferred.progress(firstProgressData);
					deferred.then(null, null, progressStub);
					deferred.progress(secondProgressData);
					assert.isTrue(progressStub.calledOnce);
					assert.equal(progressStub.lastCall.args[0], secondProgressData);
				},

				'progress() with chaining': function () {
					var expected = {};
					var innerDfd = new Deferred();
					var progressStub = sinon.stub();

					deferred
						.then(function () {
							return innerDfd;
						})
						.then(null, null, progressStub);

					deferred.resolve();
					assert.isFalse(progressStub.called);
					innerDfd.progress(expected);
					assert.isTrue(progressStub.calledOnce);
					assert.equal(progressStub.lastCall.args[0], expected);
				},

				'after progress(), the progback return value is emitted on the returned promise': function () {
					var promise = deferred.then(null, null, function (n) {
						return n * n;
					});
					var promiseStub = sinon.stub();

					promise.then(null, null, promiseStub);
					deferred.progress(2);
					assert.equal(promiseStub.lastCall.args[0], 4);
				},

				'progress() is already bound to the deferred': function () {
					var progress = deferred.progress;
					var progressStub = sinon.stub();

					deferred.then(null, null, progressStub);
					progress({});
					assert.isTrue(progressStub.called);
				}
			},
			'.cancel': {
				'cancel() invokes a canceler': function () {
					deferred.cancel();
					assert.isTrue(canceler.called);
				},

				'isCanceled() returns true after canceling': function () {
					assert.isFalse(deferred.isCanceled());
					deferred.cancel();
					assert.isTrue(deferred.isCanceled());
				},

				'isResolved() returns false after canceling': function () {
					assert.isFalse(deferred.isResolved());
					deferred.cancel();
					assert.isFalse(deferred.isResolved());
				},

				'isRejected() returns true after canceling': function () {
					assert.isFalse(deferred.isRejected());
					deferred.cancel();
					assert.isTrue(deferred.isRejected());
				},

				'isFulfilled() returns true after canceling': function () {
					assert.isFalse(deferred.isFulfilled());
					deferred.cancel();
					assert.isTrue(deferred.isFulfilled());
				},

				'cancel() is ignored after having been fulfilled': function () {
					deferred.resolve();
					deferred.cancel();
					assert.isFalse(canceler.called);
				},

				'cancel() throws error after having been fulfilled and strict': function () {
					deferred.resolve();
					assert.throws(function () {
						deferred.cancel(null, true);
					}, Error);
				},

				'cancel() without reason results in CancelError': function () {
					var reason = deferred.cancel();

					return deferred.then(null, function (result) {
						assert.equal(result, reason);
						assert.instanceOf(result, CancelError);
					});
				},

				'cancel() returns default reason': function () {
					assert.instanceOf(deferred.cancel(), CancelError);
				},

				'reason is passed to canceler': function () {
					var expected = {};

					deferred.cancel(expected);
					assert.equal(canceler.lastCall.args[0], expected);
				},

				'cancels with reason returned from canceler': function () {
					var expected = {};
					var reason = deferred.cancel(expected);

					assert.equal(reason, expected);
					return deferred.then(null, function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() returns reason from canceler': function () {
					var expected = {};

					canceler.returns(expected);
					assert.equal(deferred.cancel(), expected);
				},

				'cancel() returns reason from canceler, if canceler rejects with reason': function () {
					var expected = {};
					var canceler = function () {
						deferred.reject(expected);
						return expected;
					};
					var deferred = new Deferred(canceler);
					var reason = deferred.cancel();

					assert.equal(reason, expected);
				},

				'with canceler not returning anything, returns default CancelError': function () {
					var canceler = function () { };
					var deferred = new Deferred(canceler);
					var reason = deferred.cancel();

					return deferred.then(null, function (result) {
						assert.equal(result, reason);
						assert.instanceOf(result, CancelError);
					});
				},

				'with canceler not returning anything, still returns passed reason': function () {
					var canceler = function () { };
					var deferred = new Deferred(canceler);
					var expected = {};
					var reason = deferred.cancel(expected);

					assert.equal(reason, expected);
					return deferred.then(null, function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() does not reject promise if canceler resolves deferred': function () {
					var canceler = function () {
						deferred.resolve(expected);
					};
					var deferred = new Deferred(canceler);
					var expected = {};

					deferred.cancel();
					return deferred.then(function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() does not reject promise if canceler resolves a chain of promises': function () {
					var canceler = function () {
						deferred.resolve(expected);
					};
					var deferred = new Deferred(canceler);
					var expected = {};
					var lastPromise = deferred.then().then().then();

					lastPromise.cancel();
					assert.isTrue(deferred.isCanceled());
					assert.isTrue(lastPromise.isCanceled());

					return lastPromise.then(function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() returns undefined if canceler resolves deferred': function () {
					var canceler = function() {
						deferred.resolve({});
					};
					var deferred = new Deferred(canceler);

					assert.isUndefined(deferred.cancel());
				},

				'cancel() does not change rejection value if canceler rejects deferred': function () {
					var canceler = function () {
						deferred.reject(expected);
					};
					var deferred = new Deferred(canceler);
					var expected = {};

					deferred.cancel();
					return deferred.then(null, function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() does not change rejection value if canceler rejects a chain of promises': function () {
					var canceler = function () {
						deferred.reject(expected);
					};
					var deferred = new Deferred(canceler);
					var expected = {};
					var lastPromise = deferred.then().then().then();

					lastPromise.cancel();
					assert.isTrue(deferred.isCanceled());
					assert.isTrue(lastPromise.isCanceled());
					return lastPromise.then(null, function (result) {
						assert.equal(result, expected);
					});
				},

				'cancel() returns undefined if canceler rejects deferred': function () {
					var canceler = function () {
						deferred.reject({});
					};
					var deferred = new Deferred(canceler);

					assert.isUndefined(deferred.cancel());
				},

				'cancel() a promise chain': function () {
					var cancelerStub = sinon.stub();
					var deferred = new Deferred(cancelerStub);
					var expected = {};

					deferred.then().then().then().cancel(expected);
					assert.isTrue(cancelerStub.called);
					assert.equal(cancelerStub.lastCall.args[0], expected);
				},

				'cancel() a returned promise': function () {
					var obj = {};
					var cancelerStub = sinon.stub();
					var inner = new Deferred(cancelerStub);
					var chain = deferred.then(function () {
						return inner;
					});

					deferred.resolve();
					chain.cancel(obj, true);
					assert.isTrue(cancelerStub.calledOnce);
					assert.equal(cancelerStub.lastCall.args[0], obj);
				},

				'cancel() is already bound to the deferred': function () {
					var cancel = deferred.cancel;

					cancel();
					return deferred.then(null, function (result) {
						assert.instanceOf(result, CancelError);
					});
				}
			},

			'.then': {
				'chained then()': function () {
					function square(n) {
						return n * n;
					}

					deferred.resolve(2);
					return deferred.then(square).then(square).then(function (n) {
						assert.equal(n, 16);
					});
				},

				'asynchronously chained then()': function () {
					function asyncSquare(n) {
						var inner = new Deferred();
						setTimeout(function () {
							inner.resolve(n * n);
						}, 0);
						return inner.promise;
					}

					deferred.resolve(2);
					return deferred.then(asyncSquare).then(asyncSquare).then(function (n) {
						assert.equal(n, 16);
					});
				},

				'then() is already bound to the deferred': function () {
					var expected = {};
					var then = deferred.then;

					deferred.resolve(expected);
					return then(function (result) {
						assert.equal(result, expected);
					});
				},

				'then() with progback: returned promise is not fulfilled when progress is emitted': function () {
					var progressStub = sinon.stub();
					var promise = deferred.then(null, null, progressStub);

					deferred.progress();
					assert.isTrue(progressStub.called, 'Progress was received.');
					assert.isFalse(promise.isFulfilled(), 'Promise is not fulfilled.');
				}
			}
		};
	});
});
