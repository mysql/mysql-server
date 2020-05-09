define([
	'intern!object',
	'intern/chai!assert',
	'../../throttle',
	'sinon'
], function (registerSuite, assert, throttle, sinon) {
	registerSuite({
		name: 'dojo/throttle',

		sync: function () {
			var spy = sinon.spy();
			var throttler = sinon.spy(throttle(spy, 100));

			throttler();
			throttler();
			throttler();

			setTimeout(this.async().callback(function () {
				assert.ok(spy.callCount < throttler.callCount);
			}), 1000);
		},

		async: function () {
			var spy = sinon.spy();
			var throttler = sinon.spy(throttle(spy, 100));

			throttler();
			setTimeout(function () {
				throttler();
			}, 40);
			setTimeout(function () {
				throttler();
			}, 80);
			setTimeout(function () {
				throttler();
			}, 120);
			setTimeout(function () {
				throttler();
			}, 180);
			setTimeout(function () {
				throttler();
			}, 220);
			setTimeout(function () {
				throttler();
			}, 350);

			setTimeout(this.async().callback(function () {
				assert.ok(spy.callCount < throttler.callCount);
			}), 2000);
		}
	});
});
