define([
	'require',
	'intern!object',
	'intern/chai!assert'
], function (require, registerSuite, assert) {
	var global = this;

	registerSuite({
		name: 'undef',

		'test': function () {
			assert.ok(global.require.undef);
			assert.ok(require.undef);
		}
	});
});
