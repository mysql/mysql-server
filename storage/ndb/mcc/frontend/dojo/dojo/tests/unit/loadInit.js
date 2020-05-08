define([
	'intern!object',
	'intern/chai!assert',
	'../../loadInit',
	'../../_base/loader'
], function (registerSuite, assert, loadInit, loader) {
	registerSuite({
		name: 'dojo/loadInit',

		'construction': function () {
			assert.equal(loadInit.dynamic, 0);
			assert.isDefined(loadInit.normalize);
			assert.equal(loadInit.load, loader.loadInit);
		},

		'.normalize': function () {
			var id = Math.random();
			assert.equal(loadInit.normalize(id), id);
		}
	});
});
