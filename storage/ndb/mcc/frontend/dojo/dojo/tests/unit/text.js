define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'dojo/json'
], function (require, registerSuite, assert, JSON) {
	registerSuite({
		name: 'dojo/text',

		'no X-Requested-With header': function () {
			var dfd = this.async();

			require([ '../../text!/__services/request/xhr' ], dfd.callback(function (data) {
				data = JSON.parse(data);
				assert.ok(typeof data.headers['x-requested-with'] === 'undefined');
			}));
		}
	});
});
