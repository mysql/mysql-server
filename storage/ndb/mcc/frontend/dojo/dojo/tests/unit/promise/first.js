define([
	'intern!object',
	'intern/chai!assert',
	'dojo/Deferred',
	'../../../promise/first'
], function (
	registerSuite,
	assert,
	Deferred,
	first
) {
	var expectedResult = { a: 1, b: 'two' };

	registerSuite({
		name: 'dojo/promise/first',

		'with array argument': function () {
			var deferreds = [
				new Deferred(),
				new Deferred().resolve(expectedResult),
				{}
			];

			first(deferreds).then(this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));
		},

		'with object argument': function () {
			var deferreds = {
				a: new Deferred(),
				b: new Deferred().resolve(expectedResult),
				c: {}
			};

			first(deferreds).then(this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));
		},

		'without arguments': function () {
			first().then(this.async().callback(function () {
				assert.equal(arguments.length, 1);
				assert.isTrue(typeof arguments[0] === 'undefined');
			}));
		},

		'with single non-object argument': function () {
			first(null).then(this.async().callback(function () {
				assert.equal(arguments.length, 1);
				assert.isTrue(typeof arguments[0] === 'undefined');
			}));
		},

		'with empty array': function () {
			first([]).then(this.async().callback(function (received) {
				assert.isTrue(typeof received === 'undefined');
			}));
		},

		'with empty object': function () {
			first({}).then(this.async().callback(function (received) {
				assert.isTrue(typeof received === 'undefined');
			}));
		},

		'with one rejected promise': function () {
			var deferreds = [
				new Deferred(),
				new Deferred().reject(expectedResult),
				{}
			];

			first(deferreds).then(null, this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));
		},

		'with one promise rejected later': function () {
			var deferreds = [
				new Deferred(),
				new Deferred(),
				new Deferred()
			];

			first(deferreds).then(null, this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			}));

			deferreds[1].reject(expectedResult);
		},

		'with multiple promises rejected later': function () {
			var actualResult;
			var deferreds = [
				new Deferred(),
				new Deferred(),
				new Deferred()
			];

			first(deferreds).then(null, function (result) {
				actualResult = result;
			});

			deferreds[1].reject(expectedResult);
			deferreds[0].reject({});

			setTimeout(this.async().callback(function () {
				assert.deepEqual(actualResult, expectedResult);
			}), 0);
		},

		'cancel only affects returned promise, not those we\'re waiting for': function () {
			var othersCancelled = false;
			var onCancel = function () { othersCancelled = true; };
			var deferreds = [
				new Deferred(onCancel),
				new Deferred(onCancel),
				new Deferred(onCancel)
			];

			first(deferreds).then(null, this.async().callback(function (actualResult) {
				assert.deepEqual(actualResult, expectedResult);
			})).cancel(expectedResult);
		}

	});
});
