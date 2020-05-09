define([
	'intern!object',
	'intern/chai!assert',
	'../../../cldr/supplemental'
], function (registerSuite, assert, supplemental) {
	registerSuite({
		name: 'dojo/cldr/supplemental',

		'.getFirstDayOfWeek': function () {
			assert.strictEqual(supplemental.getFirstDayOfWeek('en-us'), 0);
			assert.strictEqual(supplemental.getFirstDayOfWeek('en-gb'), 1);
			assert.strictEqual(supplemental.getFirstDayOfWeek('es'), 1);
		},

		'.getWeekend': function () {
			assert.strictEqual(supplemental.getWeekend('en-us').start, 6);
			assert.strictEqual(supplemental.getWeekend('en').end, 0);
			assert.strictEqual(supplemental.getWeekend('he-il').start, 5);
			assert.strictEqual(supplemental.getWeekend('he').end, 6);
		}
	});
});
