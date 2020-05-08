define([
	'intern!object',
	'intern/chai!assert',
	'../../../Deferred',
	'../../../promise/tracer'
], function (
	registerSuite,
	assert,
	Deferred,
	tracer
) {
	var handle;
	var expectedResult = { a: 1, b: 'two' };
	var testDeferred;

	registerSuite({
		name: 'dojo/promise/tracer',

		beforeEach: function () {
			testDeferred = new Deferred();
		},

		afterEach: function () {
			handle && handle.remove();
			handle = undefined;
		},

		'.trace() resolved': function () {
			handle = tracer.on('resolved', this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));

			testDeferred.promise.trace();
			testDeferred.resolve(expectedResult);
		},

		'.trace() rejected': function () {
			handle = tracer.on('rejected', this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));

			testDeferred.promise.trace();
			testDeferred.reject(expectedResult);
		},

		'.trace() progress': function () {
			handle = tracer.on('progress', this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));

			testDeferred.promise.trace();
			testDeferred.progress(expectedResult);
		},

		'.trace() passing extra arguments': function () {
			handle = tracer.on('resolved', this.async().callback(function (actualResult, arg1, arg2) {
				assert.deepEqual(actualResult, expectedResult);
				assert.equal(arg1, 'test');
				assert.deepEqual(arg2, expectedResult);
			}));

			testDeferred.promise.trace('test', expectedResult);
			testDeferred.resolve(expectedResult);
		},

		'.traceRejected()': function () {
			handle = tracer.on('rejected', this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));

			testDeferred.promise.traceRejected();
			testDeferred.reject(expectedResult);
		},

		'.traceRejected() passing extra arguments': function () {
			handle = tracer.on('rejected', this.async().callback(function (actualResult, arg1, arg2) {
				assert.deepEqual(actualResult, expectedResult);
				assert.equal(arg1, 'test');
				assert.deepEqual(arg2, expectedResult);
			}));

			testDeferred.promise.traceRejected('test', expectedResult);
			testDeferred.reject(expectedResult);
		}
	});
});
