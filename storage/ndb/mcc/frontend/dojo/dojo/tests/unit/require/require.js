define([
	'intern!object',
	'intern/chai!assert',
	'require'
], function (
	registerSuite,
	assert,
	require
) {

	registerSuite({
		name: 'dojo/require',

		afterEach: function () {
			window.modA = undefined;
			window.modB = undefined;
			window.modC = undefined;
		},

		'single legacy module': function () {
			var dfd = this.async();
			require(['../../../require!./support/testModA'], dfd.callback(function () {
				assert.isTrue(window.modA, 'module A should be loaded');
			}));
		},

		'multiple legacy module': function () {
			var dfd = this.async();
			require(['../../../require!./support/testModB,./support/testModC'], dfd.callback(function () {
				assert.isTrue(window.modB, 'module B should be loaded');
				assert.isTrue(window.modC, 'module C should be loaded');
			}));
		}
	});
});
