define([
	'intern!object',
	'intern/chai!assert',
	'dojo/_base/array',
	'../../../_base/Color',
	'dojo/has!host-browser?dojo/domReady!'
], function (registerSuite, assert, array, Color) {
	var white  = Color.fromString('white').toRgba();
	var maroon = Color.fromString('maroon').toRgba();
	var verifyColor = function(source, expected){
		var color = new Color(source);
		assert.deepEqual(color.toRgba(), expected);
		array.forEach(color.toRgba(), function(n){
			assert.typeOf(n, 'number');
		});
	};

	registerSuite({
		name: 'dojo/_base/Color',

		'maroon string': function () {
			verifyColor('maroon', maroon);
		},

		'white string': function () {
			verifyColor('white', white);
		},

		'white hex short': function () {
			verifyColor('#fff', white);
		},

		'white hex': function () {
			verifyColor('#ffffff', white);
		},

		'white rgb': function () {
			verifyColor('rgb(255,255,255)', white);
		},

		'maroon hex': function () {
			verifyColor('#800000', maroon);
		},

		'maroon rgb': function () {
			verifyColor('rgb(128, 0, 0)', maroon);
		},

		'aliceblue rgba': function () {
			verifyColor('rgba(128, 0, 0, 0.5)', [128, 0, 0, 0.5]);
		},

		'maroon rgba == rgba': function () {
			verifyColor(maroon, maroon);
		},

		'rgb alpha': function () {
			verifyColor([1, 2, 3], [1, 2, 3, 1]);
		},

		'array': function () {
			verifyColor([1, 2, 3, 0.5], [1, 2, 3, 0.5]);
		},

		'blend black and white': function () {
			verifyColor(Color.blendColors(new Color('black'), new Color('white'), 0.5), [128, 128, 128, 1]);
		}
	});
});
