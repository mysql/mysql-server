define([
	'intern!object',
	'intern/chai!assert',
	'../../debounce',
	'sinon'
], function (registerSuite, assert, debounce, sinon) {
	registerSuite({
		name: 'dojo/debounce',

		sync: function () {
			var spy = sinon.spy();
			var debouncer = debounce(spy, 100);

			debouncer();
			debouncer();
			debouncer();

			setTimeout(this.async().callback(function () {
				assert.equal(spy.callCount, 1);
			}), 1000);
		},

		async: function () {
			var spy = sinon.spy();
			var debouncer = sinon.spy(debounce(spy, 100));

			debouncer();
			setTimeout(function () {
				debouncer();
			}, 40);
			setTimeout(function () {
				debouncer();
			}, 80);
			setTimeout(function () {
				debouncer();
			}, 120);
			setTimeout(function () {
				debouncer();
			}, 180);
			setTimeout(function () {
				debouncer();
			}, 220);
			setTimeout(function () {
				debouncer();
			}, 350);

			setTimeout(this.async().callback(function () {
				assert.ok(spy.callCount < debouncer.callCount);
			}), 2000);
		}
	});
});
