define([
	'intern!object',
	'intern/chai!assert',
	'../../back',
	'../../has',
	'../../sniff'
], function (registerSuite, assert, back, has) {
	registerSuite({
		name: 'dojo/back',

		'getHash and setHash': function () {
			var cases = [
				'test',
				'test with spaces',
				'test%20with%20encoded',
				'test+with+pluses',
				' leading',
				'trailing ',
				'under_score',
				'extra#mark',
				'extra?instring',
				'extra&instring',
				'#leadinghash'
			];
			var str;
			var hasMozilla = has('mozilla');

			for (var i = cases.length; i--;) {
				str = cases[i];
				back.setHash(str);
				if (hasMozilla) {
					assert.strictEqual(str, decodeURIComponent(back.getHash()));
				} else {
					assert.strictEqual(str, back.getHash());
				}
			}
		}
	});
});
