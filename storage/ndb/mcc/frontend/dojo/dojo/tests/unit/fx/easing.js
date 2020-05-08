define([
	'intern!object',
	'intern/chai!assert',
	'../../../fx/easing'
], function (registerSuite, assert, easing) {
	registerSuite({
		name: 'dojo/fx/easing',

		'module': {
			'full of functions': function () {
				for(var i in easing){
					assert.isFunction(easing[i]);
				}
			}
		},

		'performs some calculation': function () {
			for(var i in easing){
				assert.isFalse(isNaN(easing[i](0.5)));
			}
		}
	});
});
