define([
	'intern!object',
	'intern/chai!assert',
	'../../number',
	'../../i18n'
], function (registerSuite, assert, number) {

	var defaultLocale = 'en-us';

	function assertStrictNaN(value) {
		// reliable test for NaN (not subject to coersion)
		assert.notStrictEqual(value, value);
	}

	function isStrictNaN(value) {
		// reliable test for NaN (not subject to coercion)
		// isNaN(undefined) return true in Chrome 40
		return value !== value;
	}

	function decimalNumberDiff(num1, num2) {
		//TODO: should be more accurate when dojo/number finish rounding in the future
		var diffBound = 1e-3;
		var diff = num1 - num2;

		if (Math.abs(diff) < diffBound) {
			return true;
		}
		else if (isNaN(Math.abs(diff))) {
			var s = num1.toString().split(num2)[1];
			s = s.replace(',', '0').replace('\u066b', '0');
			return (Number(s) < diffBound);
		}
		return false;
	}

	function checkFormatParseCycle(options, sourceInput, expected, backwardCheck) {
		// backwardCheck is a boolean indicating whether test needs a roundtrip, e.g. format->parse->format
		if (options == null) {
			var pattern = options.pattern;
			var locale = options.locale;
			//TODO: add more fields
		}

		var str = pattern == null ? 'default' : pattern;
		var result = number.format(sourceInput, options);
		if (isStrictNaN(expected)) {
			assertStrictNaN(result);
		} else {
			assert.strictEqual(result, expected);
		}
		if (backwardCheck) {
			var resultParsed = number.parse(result,options);
			if (!decimalNumberDiff(resultParsed, sourceInput)) {
			    assert.strictEqual(resultParsed, sourceInput);
			}
			var resultParsedReformatted = number.format(resultParsed, options);
		    if (!decimalNumberDiff(result, resultParsedReformatted)) {
				assert.strictEqual(resultParsedReformatted, result);
			}
		}
	}

	function checkParse(options, sourceInput, expected) {
		var str = 'default';
		if (options && options.pattern != null) {
			str = options.pattern;
		}
		//print('input:' + sourceInput);
		var result = number.parse(sourceInput, options);
		//print('result :' + result);
		if (isStrictNaN(expected)) {
			assertStrictNaN(result);
		} else {
			assert.strictEqual(result, expected);
		}
	}

	function checkRounding(num, maxFractionDigits, expected, locale) {
		var pattern = '#0.';
		for (var i = 0; i < maxFractionDigits; i++) {
			pattern += '#';
		}
		var result = number.format(num,{locale: locale || defaultLocale, pattern:pattern});
		assert.strictEqual(result, expected);
	}

	registerSuite({
		name: 'dojo/number',

		// TODO: setup function to load locales?

		'.invalid': function () {
			assert.isNull(number.format(NaN));
			assert.isNull(number.format(Number.NaN));
			assert.isNull(number.format(Infinity));
			assert.isNull(number.format(-Infinity));
		},

		'.round': {
			basic: function () {
				assert.strictEqual(number.round(0), 0);
				assert.strictEqual(number.round(0.5), 1);
				assert.strictEqual(number.round(-0.5), -1);
				assert.strictEqual(number.round(0.05, 1), 0.1);
				assert.strictEqual(number.round(0.09, 1), 0.1);
				assert.strictEqual(number.round(0.04999999, 1), 0.0);
				assert.strictEqual(number.round(0.09499999, 1), 0.1);
				assert.strictEqual(number.round(0.095, 1), 0.1);
				assert.strictEqual(number.round(0.09999999, 1), 0.1);
				assert.strictEqual(number.round(-0.05, 1), -0.1);
				assert.strictEqual(number.round(1.05, 1), 1.1);
				assert.strictEqual(number.round(-1.05, 1), -1.1);
				// assert.strictEqual(number.round(-162.295, 2), -162.29); // see ticket #7930, dojox.math.round
				// assert.strictEqual(number.round(162.295, 2), 162.29); // ibid
			},

			multiple: function() {
				assert.strictEqual(number.round(123.4525, 2, 5), 123.455);
				assert.strictEqual(number.round(123.452, 2, 5), 123.45);
				assert.strictEqual(number.round(123.454, 2, 5), 123.455);
				assert.strictEqual(number.round(123.456, 2, 5), 123.455);
				assert.strictEqual(number.round(-123.452, 2, 5), -123.45);
				assert.strictEqual(number.round(-123.4525, 2, 5), -123.455);
				assert.strictEqual(number.round(-123.454, 2, 5), -123.455);
				assert.strictEqual(number.round(-123.456, 2, 5), -123.455);
			},

			speleotrove: function() {
				// submitted Mike Cowlishaw (IBM, CCLA), see http://speleotrove.com/decimal/#testcases
				assert.strictEqual(number.round(12345 + -0.1), 12345, 'radx200');
				assert.strictEqual(number.round(12345 + -0.01), 12345, 'radx201');
				assert.strictEqual(number.round(12345 + -0.001), 12345, 'radx202');
				assert.strictEqual(number.round(12345 + -0.00001), 12345, 'radx203');
				assert.strictEqual(number.round(12345 + -0.000001), 12345, 'radx204');
				assert.strictEqual(number.round(12345 + -0.0000001), 12345, 'radx205');
				assert.strictEqual(number.round(12345 + 0), 12345, 'radx206');
				assert.strictEqual(number.round(12345 + 0.0000001), 12345, 'radx207');
				assert.strictEqual(number.round(12345 + 0.000001), 12345, 'radx208');
				assert.strictEqual(number.round(12345 + 0.00001), 12345, 'radx209');
				assert.strictEqual(number.round(12345 + 0.0001), 12345, 'radx210');
				assert.strictEqual(number.round(12345 + 0.001), 12345, 'radx211');
				assert.strictEqual(number.round(12345 + 0.01), 12345, 'radx212');
				assert.strictEqual(number.round(12345 + 0.1), 12345, 'radx213');

				assert.strictEqual(number.round(12346 + 0.49999), 12346, 'radx215');
				assert.strictEqual(number.round(12346 + 0.5), 12347, 'radx216');
				assert.strictEqual(number.round(12346 + 0.50001), 12347, 'radx217');

				assert.strictEqual(number.round(12345 + 0.4), 12345, 'radx220');
				assert.strictEqual(number.round(12345 + 0.49), 12345, 'radx221');
				assert.strictEqual(number.round(12345 + 0.499), 12345, 'radx222');
				assert.strictEqual(number.round(12345 + 0.49999), 12345, 'radx223');
				assert.strictEqual(number.round(12345 + 0.5), 12346, 'radx224');
				assert.strictEqual(number.round(12345 + 0.50001), 12346, 'radx225');
				assert.strictEqual(number.round(12345 + 0.5001), 12346, 'radx226');
				assert.strictEqual(number.round(12345 + 0.501), 12346, 'radx227');
				assert.strictEqual(number.round(12345 + 0.51), 12346, 'radx228');
				assert.strictEqual(number.round(12345 + 0.6), 12346, 'radx229');

				//negatives
				assert.strictEqual(number.round(-12345 + -0.1), -12345, 'rsux200');
				assert.strictEqual(number.round(-12345 + -0.01), -12345, 'rsux201');
				assert.strictEqual(number.round(-12345 + -0.001), -12345, 'rsux202');
				assert.strictEqual(number.round(-12345 + -0.00001), -12345, 'rsux203');
				assert.strictEqual(number.round(-12345 + -0.000001), -12345, 'rsux204');
				assert.strictEqual(number.round(-12345 + -0.0000001), -12345, 'rsux205');
				assert.strictEqual(number.round(-12345 + 0), -12345, 'rsux206');
				assert.strictEqual(number.round(-12345 + 0.0000001), -12345, 'rsux207');
				assert.strictEqual(number.round(-12345 + 0.000001), -12345, 'rsux208');
				assert.strictEqual(number.round(-12345 + 0.00001), -12345, 'rsux209');
				assert.strictEqual(number.round(-12345 + 0.0001), -12345, 'rsux210');
				assert.strictEqual(number.round(-12345 + 0.001), -12345, 'rsux211');
				assert.strictEqual(number.round(-12345 + 0.01), -12345, 'rsux212');
				assert.strictEqual(number.round(-12345 + 0.1), -12345, 'rsux213');

				assert.strictEqual(number.round(-12346 + 0.49999), -12346, 'rsux215');
				assert.strictEqual(number.round(-12346 + 0.5), -12346, 'rsux216');
				assert.strictEqual(number.round(-12346 + 0.50001   ), -12345, 'rsux217');

				assert.strictEqual(number.round(-12345 + 0.4), -12345, 'rsux220');
				assert.strictEqual(number.round(-12345 + 0.49), -12345, 'rsux221');
				assert.strictEqual(number.round(-12345 + 0.499), -12345, 'rsux222');
				assert.strictEqual(number.round(-12345 + 0.49999), -12345, 'rsux223');
				assert.strictEqual(number.round(-12345 + 0.5), -12345, 'rsux224');
				assert.strictEqual(number.round(-12345 + 0.50001), -12344, 'rsux225');
				assert.strictEqual(number.round(-12345 + 0.5001), -12344, 'rsux226');
				assert.strictEqual(number.round(-12345 + 0.501), -12344, 'rsux227');
				assert.strictEqual(number.round(-12345 + 0.51), -12344, 'rsux228');
				assert.strictEqual(number.round(-12345 + 0.6), -12344, 'rsux229');

				assert.strictEqual(number.round(12345 /  1), 12345, 'rdvx401');
				assert.strictEqual(number.round(12345 /  1.0001), 12344, 'rdvx402');
				assert.strictEqual(number.round(12345 /  1.001), 12333, 'rdvx403');
				assert.strictEqual(number.round(12345 /  1.01), 12223, 'rdvx404');
				assert.strictEqual(number.round(12345 /  1.1), 11223, 'rdvx405');

				assert.strictEqual(number.round(12355 / 4, 1), 3088.8, 'rdvx406');
				assert.strictEqual(number.round(12345 / 4, 1), 3086.3, 'rdvx407');
				assert.strictEqual(number.round(12355 / 4.0001, 1), 3088.7, 'rdvx408');
				assert.strictEqual(number.round(12345 / 4.0001, 1), 3086.2, 'rdvx409');
				assert.strictEqual(number.round(12345 / 4.9, 1), 2519.4, 'rdvx410');
				assert.strictEqual(number.round(12345 / 4.99, 1), 2473.9, 'rdvx411');
				assert.strictEqual(number.round(12345 / 4.999, 1), 2469.5, 'rdvx412');
				assert.strictEqual(number.round(12345 / 4.9999, 1), 2469.0, 'rdvx413');
				assert.strictEqual(number.round(12345 / 5, 1), 2469, 'rdvx414');
				assert.strictEqual(number.round(12345 / 5.0001, 1), 2469.0, 'rdvx415');
				assert.strictEqual(number.round(12345 / 5.001, 1), 2468.5, 'rdvx416');
				assert.strictEqual(number.round(12345 / 5.01, 1), 2464.1, 'rdvx417');
				assert.strictEqual(number.round(12345 / 5.1, 1), 2420.6, 'rdvx418');

				assert.strictEqual(number.round(12345 * 1), 12345, 'rmux401');
				assert.strictEqual(number.round(12345 * 1.0001), 12346, 'rmux402');
				assert.strictEqual(number.round(12345 * 1.001), 12357, 'rmux403');
				assert.strictEqual(number.round(12345 * 1.01), 12468, 'rmux404');
				assert.strictEqual(number.round(12345 * 1.1), 13580, 'rmux405');
				assert.strictEqual(number.round(12345 * 4), 49380, 'rmux406');
				assert.strictEqual(number.round(12345 * 4.0001), 49381, 'rmux407');
				assert.strictEqual(number.round(12345 * 4.9), 60491, 'rmux408');
				assert.strictEqual(number.round(12345 * 4.99), 61602, 'rmux409');
				assert.strictEqual(number.round(12345 * 4.999), 61713, 'rmux410');
				assert.strictEqual(number.round(12345 * 4.9999), 61724, 'rmux411');
				assert.strictEqual(number.round(12345 * 5), 61725, 'rmux412');
				assert.strictEqual(number.round(12345 * 5.0001), 61726, 'rmux413');
				assert.strictEqual(number.round(12345 * 5.001), 61737, 'rmux414');
				assert.strictEqual(number.round(12345 * 5.01), 61848, 'rmux415');

				// assert.strictEqual(number.round(12345 * 12), 1.4814E+5, 'rmux416');
				// assert.strictEqual(number.round(12345 * 13), 1.6049E+5, 'rmux417');
				// assert.strictEqual(number.round(12355 * 12), 1.4826E+5, 'rmux418');
				// assert.strictEqual(number.round(12355 * 13), 1.6062E+5, 'rmux419');
			}
		},

		'.format': {
			'old tests': function () {
				assert.strictEqual(number.format(123, {pattern: '0000'}), '0123');
				assert.strictEqual(number.format(-1234567.89, {pattern: '#,##,##0.000##', locale: 'en-us'}), '-12,34,567.890');
				assert.strictEqual(number.format(-1234567.890123, {pattern: '#,##,##0.000##', locale: 'en-us'}), '-12,34,567.89012');
				assert.strictEqual(number.format(-1234567.890123, {pattern: '#,##0.000##;(#,##0.000##)', locale: 'en-us'}), '(1,234,567.89012)');
				assert.strictEqual(number.format(-1234567.890123, {pattern: '#,##0.000##;(#)', locale: 'en-us'}), '(1,234,567.89012)');
				assert.strictEqual(number.format(0.501, {pattern: '#0.#%', locale: 'en-us'}), '50.1%');
				assert.strictEqual(number.format(1998, {pattern: '00'}), '98');
				assert.strictEqual(number.format(1998, {pattern: '00000'}), '01998');
				assert.strictEqual(number.format(0.125, {pattern: '0.##', locale: 'en-us'}), '0.13'); //NOTE: expects round_half_up, not round_half_even
				assert.strictEqual(number.format(0.125, {pattern: '0.0000', locale: 'en-us'}), '0.1250');
				assert.strictEqual(number.format(0.100004, {pattern: '0.####', locale: 'en-us'}), '0.1');

				assert.strictEqual(number.format(-12.3, {places:0, locale: 'en-us'}), '-12');
				assert.strictEqual(number.format(-1234567.89, {locale: 'en-us'}), '-1,234,567.89');
				// assert.strictEqual(number.format(-1234567.89, {locale: 'en-in'}), '-12,34,567.89');
				assert.strictEqual(number.format(-1234567.89, {places:0, locale: 'en-us'}), '-1,234,568');
				// assert.strictEqual(number.format(-1234567.89, {places:0, locale: 'en-in'}), '-12,34,568');
				assert.strictEqual(number.format(-1000.1, {places:2, locale: 'fr-fr'}), '-1\xa0000,10');
				assert.strictEqual(number.format(-1000.1, {places:2, locale: 'en-us'}), '-1,000.10');
				assert.strictEqual(number.format(-1000.1, {places:2, locale: 'fr-fr'}), '-1\xa0000,10');
				assert.strictEqual(number.format(-1234.56, {places:2, locale: 'de-de'}), '-1.234,56');
				assert.strictEqual(number.format(-1000.1, {places:2, locale: 'en-us'}), '-1,000.10');
				assert.strictEqual(number.format(1.23456, {places:2, locale: 'en-us', type: 'percent'}), '123.46%');
				assert.strictEqual(number.format(123.4, {places:'1,3', locale: 'en-us'}), '123.4');
				assert.strictEqual(number.format(123.45, {places:'1,3', locale: 'en-us'}), '123.45');
				assert.strictEqual(number.format(123.456, {places:'1,3', locale: 'en-us'}), '123.456');

				// rounding
				assert.strictEqual(number.format(-1234567.89, {places:0, locale: 'en-us'}), '-1,234,568');
				// assert.strictEqual(number.format(-1234567.89, {places:0, locale: 'en-in'}), '-12,34,568');
				assert.strictEqual(number.format(-1000.114, {places:2, locale: 'en-us'}), '-1,000.11');
				assert.strictEqual(number.format(-1000.115, {places:2, locale: 'en-us'}), '-1,000.12');
				assert.strictEqual(number.format(-1000.116, {places:2, locale: 'en-us'}), '-1,000.12');
				assert.strictEqual(number.format(-0.0001, {places:2, locale: 'en-us'}), '-0.00');
				assert.strictEqual(number.format(0, {places:2, locale: 'en-us'}), '0.00');

				// change decimal places
				assert.strictEqual(number.format(-1000.1, {places:3, locale: 'fr-fr'}), '-1\xa0000,100');
				assert.strictEqual(number.format(-1000.1, {places:3, locale: 'en-us'}), '-1,000.100');
			},

			patterns: function() {
				var patterns = ([ '#0.#', '#0.', '#.0', '#' ]);
				var patternsLength = patterns.length;
				var num = ([ '0', '0', '0.0', '0' ]);
				var options;
				//icu4j result seems doesn't work as:
				//var num = (['0','0.', '.0', '0']);
				for (var i=0; i<patternsLength; ++i) {
					options = {pattern:patterns[i], locale: 'en-us'};
					checkFormatParseCycle(options, 0, num[i], false);
				}
			},

			rounding: function() {
				checkRounding(0.000179999, 5, '0.00018');
				checkRounding(0.00099, 4, '0.001');
				checkRounding(17.6995, 3, '17.7');
				checkRounding(15.3999, 0, '15');
				checkRounding(-29.6, 0, '-30');
			},

			perMill: function() {
				var result = number.format(0.4857,{pattern: '###.###\u2030', locale: 'en-us'});
				assert.strictEqual(result, '485.7\u2030');
			},

			grouping: function() {
				var options = {pattern:'#,##,###',locale:'en-us'};
				//step1: 123456789 formated=> 12,34,56,789
				//step2:12,34,56,789 parsed=> 123456789 => formated => 12,34,56,789
				checkFormatParseCycle(options, 123456789, '12,34,56,789', true);
			}
		},

		'.parse': {
			'old tests': function() {
				assert.strictEqual(number.parse('1000', {locale: 'en-us'}), 1000);
				assert.strictEqual(number.parse('1000.123', {locale: 'en-us'}), 1000.123);
				assert.strictEqual(number.parse('1,000', {locale: 'en-us'}), 1000);
				assert.strictEqual(number.parse('-1000', {locale: 'en-us'}), -1000);
				assert.strictEqual(number.parse('-1000.123', {locale: 'en-us'}), -1000.123);
				assert.strictEqual(number.parse('-1,234,567.89', {locale: 'en-us'}), -1234567.89);
				assert.strictEqual(number.parse('-1 234 567,89', {locale: 'fr-fr'}), -1234567.89);
				assertStrictNaN(number.parse('-1 234 567,89', {locale: 'en-us'}));

				assert.strictEqual(number.parse('0123', {pattern: '0000'}), 123);

				assertStrictNaN(number.parse('10,00', {locale: 'en-us'}));
				assertStrictNaN(number.parse('1000.1', {locale: 'fr-fr'}));

				assertStrictNaN(number.parse(''));
				assertStrictNaN(number.parse('abcd'));

				// should allow unlimited precision, by default
				assert.strictEqual(number.parse('1.23456789', {locale: 'en-us'}), 1.23456789);

				// test whitespace
				// assert.strictEqual(-1234567, number.parse('  -1,234,567  ', {locale: 'en-us'}));

				// assert.ok(number.parse('9.1093826E-31'));
				assert.strictEqual(number.parse('123%', {locale: 'en-us', type: 'percent'}), 1.23);
				assert.strictEqual(number.parse('123%', {places:0, locale: 'en-us', type: 'percent'}), 1.23);
				assertStrictNaN(number.parse('123.46%', {places:0, locale: 'en-us', type: 'percent'}));
				assert.strictEqual(number.parse('123.46%', {places:2, locale: 'en-us', type: 'percent'}), 1.2346);
				assert.strictEqual(number.parse('50.1%', {pattern: '#0.#%', locale: 'en-us'}), 0.501);

				assert.strictEqual(number.parse('123.4', {pattern: '#0.#', locale: 'en-us'}), 123.4);
				assert.strictEqual(number.parse('-123.4', {pattern: '#0.#', locale: 'en-us'}), -123.4);
				assert.strictEqual(number.parse('123.4', {pattern: '#0.#;(#0.#)', locale: 'en-us'}), 123.4);
				assert.strictEqual(number.parse('(123.4)', {pattern: '#0.#;(#0.#)', locale: 'en-us'}), -123.4);

				assert.isNull(number.format('abcd', {pattern: '0000'}));

				assert.strictEqual(number.parse('123', {places: 0}), 123);
				assert.strictEqual(number.parse('123', {places:'0'}), 123);
				assert.strictEqual(number.parse('123.4', {places:1, locale: 'en-us'}), 123.4);
				assert.strictEqual(number.parse('123.45', {places:'1,3', locale: 'en-us'}), 123.45);
				assert.strictEqual(number.parse('123.45', {places:'0,2', locale: 'en-us'}), 123.45);
			},
			't18466': function () {
				var locale = "fr";
				checkParse({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1,00 ", NaN);
				checkParse({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1,00", 1);
				checkParse({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1,00 ", NaN);
				checkParse({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1,00", 1);
				checkParse({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1 000,00 ", NaN);
				checkParse({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1 000,00", 1000);
				checkParse({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1 000,00 ", NaN);
				checkParse({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1 000,00", 1000);
				checkFormatParseCycle({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1200", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1200", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "#,###.00 ¤;(#,###.00) ¤", locale: locale }, "1200 ", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "#,###.00¤;(#,###.00)¤", locale: locale }, "1200 ", "1\xa0200,00", true)

				checkParse({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, " 1,00", NaN);
				checkParse({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, "1,00", 1);
				checkParse({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, " 1,00", NaN);
				checkParse({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, "1,00", 1);
				checkParse({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, " 1 000,00", NaN);
				checkParse({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, "1 000,00", 1000);
				checkParse({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, " 1 000,00", NaN);
				checkParse({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, "1 000,00", 1000);
				checkFormatParseCycle({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, "1200", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, "1200", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "¤ #,###.00;¤ (#,###.00)", locale: locale }, " 1200", "1\xa0200,00", true)
				checkFormatParseCycle({ pattern: "¤#,###.00;¤(#,###.00)", locale: locale }, " 1200 ", "1\xa0200,00", true)


			}
		},

		// These tests refer to specific tests in ICU4J
		'ICU4J Regressions': function () {

			// NumberFormatRegressionTest.Test4161100
			checkFormatParseCycle({ pattern: '#0.#', locale: 'en-us' }, -0.09, '-0.1', false);


			// NumberRegression.Test4087535, NumberRegression.Test4243108
			checkFormatParseCycle({ places: 0 }, 0, '0', false);
			// TODO: in icu4j,0.1 should be formatted to '.1' when minimumIntegerDigits=0
			checkFormatParseCycle({ places: 0 }, 0.1, '0', false);
			checkParse({ pattern: '#0.#####', locale: 'en-us'}, 123.55456,123.55456);
			//!! fails because default pattern only has 3 decimal places
			// checkParse(null, 123.55456, 123.55456);

			// See whether it fails first format 0.0 ,parse '99.99',and then reformat 0.0
			checkFormatParseCycle({ pattern: '#.#' }, 0.0, '0', false);
			checkParse({ locale: 'en-us' }, '99.99', 99.99);
			checkFormatParseCycle({ pattern: '#.#' }, 0.0, '0', false);


			// NumberRegression.Test4088503, NumberRegression.Test4106658
			checkFormatParseCycle({ places: 0 }, 123, '123', false);

			// TODO: differernt from ICU where -0.0 is formatted to '-0'
			checkFormatParseCycle({ locale: 'en-us' }, -0.0, '0', false);

			// TODO: differernt from ICU where -0.0001 is formatted to '-0'
			checkFormatParseCycle({ locale: 'en-us', places: 6 }, -0.0001, '-0.000100', false);


			// NumberRegression.Test4086575
			var locale = 'fr';
			var pattern = '###.00;(###.00)';
			var options = { pattern:pattern,locale: locale };

			// no group separator
			checkFormatParseCycle(options, 1234, '1234,00', false);
			checkFormatParseCycle(options, -1234, '(1234,00)', false);

			// space as group separator
			pattern = '#,###.00;(#,###.00)';
			options = { pattern: pattern, locale: locale };
			checkFormatParseCycle(options, 1234, '1\u00a0234,00', false); // Expect 1 234,00
			checkFormatParseCycle(options, -1234, '(1\u00a0234,00)', false); // Expect (1 234,00)


			// NumberRegression.Test4092480, NumberRegression.Test4074454
			var patterns = ([ '#0000', '#000', '#00', '#0', '#' ]);
			var expected = ([ '0042', '042', '42', '42', '42' ]);

			for (var i = 0; i < patterns.length; i ++) {
				checkFormatParseCycle({ pattern: patterns[i] }, 42, expected[i], false);
				checkFormatParseCycle({ pattern: patterns[i] }, -42, '-' + expected[i], false);
			}

			checkFormatParseCycle({ pattern : '#,#00.00;-#.#', locale: 'en-us' },3456.78,'3,456.78',false);
			//!!Failed case
			// checkFormatParseCycle({pattern:'#,#00.00 p''ieces;-#,#00.00 p''ieces'},3456.78,'3,456.78 p'ieces',false);
			// checkFormatParseCycle({pattern:'000.0#0'},3456.78,null,false);
			// checkFormatParseCycle({pattern:'0#0.000'},3456.78,null,false);


			// NumberRegression.Test4052223
			checkParse({ pattern: '#,#00.00' }, 'abc3', NaN);

			//TODO: got NaN instead of 1.222, is it ok?
			//checkParse({pattern:'#,##0.###',locale:'en-us'},'1.222,111',1.222);
			//checkParse({pattern:'#,##0.###',locale:'en-us'},'1.222x111',1.222);

			//got NaN for illegal input, ok
			checkParse(null,'hello: ,.#$@^&**10x', NaN);


			// NumberRegression.Test4125885
			checkFormatParseCycle({ pattern: '000.00', locale: 'en-us' }, 12.34, '012.34', false);
			checkFormatParseCycle({ pattern: '+000.00%;-000.00%', locale: 'en-us' }, 0.1234, '+012.34%', false);
			checkFormatParseCycle({ pattern: '##,###,###.00', locale: 'en-us'}, 9.02, '9.02', false);

			var patterns = ([ '#.00', '0.00', '00.00', '#0.0#', '#0.00' ]);
			var expected = ([ '1.20', '1.20', '01.20', '1.2', '1.20' ]);
			for (var i = 0; i < patterns.length; i++){
				checkFormatParseCycle({ pattern: patterns[i], locale: 'en-us' }, 1.2, expected[i], false);
			}
		}
	});
});
