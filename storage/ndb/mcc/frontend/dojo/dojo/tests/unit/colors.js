define([
	'intern!object',
	'intern/chai!assert',
	'../../_base/Color',
	'../../colors'
], function (registerSuite, assert, Color, colors) {

	/**
	 * array or arrays to add for tests - actual and expected indexes
	 * @type {Array}
	 */
	var testColorsObject = [
		[ 'black', [0, 0, 0] ],
		[ 'white', [255, 255, 255] ],
		[ 'maroon', [128, 0, 0] ],
		[ 'olive', [128, 128, 0] ],
		[ '#f00', 'red' ],
		[ '#ff0000', 'red' ],
		[ 'rgb(255, 0, 0)', 'red' ],
		[ 'rgb(100%, 0%, 0%)', 'red' ],
		[ 'rgb(300, 0, 0)', 'red' ],
		[ 'rgb(255, -10, 0)', 'red' ],
		[ 'rgb(110%, 0%, 0%)', 'red' ],
		[ 'rgba(255, 0, 0, 1)', 'red' ],
		[ 'rgba(100%, 0%, 0%, 1)', 'red' ],
		[ 'rgba(0, 0, 255, 0.5)', [0, 0, 255, 0.5] ],
		[ 'rgba(100%, 50%, 0%, 0.1)', [255, 128, 0, 0.1] ],
		[ 'hsl(0, 100%, 50%)', 'red' ],
		[ 'hsl(120, 100%, 50%)', 'lime' ],
		[ 'hsl(120, 100%, 25%)', 'green' ],
		[ 'hsl(120, 100%, 75%)', '#80ff80' ],
		[ 'hsl(120, 50%, 50%)', '#40c040' ],
		[ 'hsla(120, 100%, 50%, 1)', 'lime' ],
		[ 'hsla(240, 100%, 50%, 0.5)', [0, 0, 255, 0.5] ],
		[ 'hsla(30, 100%, 50%, 0.1)', [255, 128, 0, 0.1] ],
		[ 'transparent', [0, 0, 0, 0] ],
		[ colors.makeGrey(5), [5, 5, 5, 1] ],
		[ colors.makeGrey(2, 0.3), [2, 2, 2, 0.3] ]
	];

	registerSuite({
		name: 'dojo/colors',

		'test colors': function () {
			var i,
				k,
				actual,
				expected;

			for (i = 0; i < testColorsObject.length; i += 1) {
				actual = new Color(testColorsObject[i][0]);
				expected = new Color(testColorsObject[i][1]);

				assert.deepEqual(actual.toRgba(), expected.toRgba());

				for (k = 0; k < actual.length; k += 1) {
					assert.isNumber(actual[k]);
				}
			}
		}
	});
});
