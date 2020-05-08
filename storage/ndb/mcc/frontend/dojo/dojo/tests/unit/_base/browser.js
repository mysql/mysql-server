define([
	'intern!object',
	'intern/chai!assert',
	'../../../_base/browser',
	'../../../ready'
], function (
	registerSuite,
	assert,
	browser,
	ready
) {

	registerSuite({
		name: 'dojo/_base/browser',

		'is a dojo/ready function': function () {
			assert.strictEqual(ready, browser);
		}
	});

});
