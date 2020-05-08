define([
	'intern!object',
	'intern/chai!assert',
	'../../currency',
	'dojo/i18n'
], function (registerSuite, assert, currency) {
	registerSuite({
		name: 'dojo/currency',

		'.format': {
			'en-us locale': {
				'EUR currency': function () {
					assert.equal(currency.format(123.45, {currency: 'EUR', locale: 'en-us'}), '\u20ac123.45');
				},

				'USD currency': {
					'hundreds': function () {
						assert.equal(currency.format(123.45, {currency: 'USD', locale: 'en-us'}), '$123.45');
					},

					'thousands separator': function () {
						assert.equal(currency.format(1234.56, {currency: 'USD', locale: 'en-us'}), '$1,234.56');
					},

					'fractional is false': function () {
						var options = {currency: 'USD', fractional: false, locale: 'en-us'};
						assert.equal(currency.format(1234, options), '$1,234');
					}
				},

				'CAD currency': function () {
					assert.equal(currency.format(123.45, {currency: 'CAD', locale: 'en-us'}), 'CA$123.45');
				},

				'unknown currency': function () {
					// There is no special currency symbol for ADP, so expect the ISO code instead
					assert.equal(currency.format(123, {currency: 'ADP', locale: 'en-us'}), 'ADP123');
				}
			},

			'en-ca locale': {
				'USD currency': function () {
					assert.equal(currency.format(123.45, {currency: 'USD', locale: 'en-ca'}), 'US$123.45');
				},

				'CAD currency': function () {
					assert.equal(currency.format(123.45, {currency: 'CAD', locale: 'en-ca'}), '$123.45');
				}
			},

			'de-de locale': {
				'EUR currency': {
					'hundreds': function () {
						assert.equal(currency.format(123.45, {currency: 'EUR', locale: 'de-de'}), '123,45\xa0\u20ac');
					},

					'thousands': function () {
						var expected = '1.234,56\xa0\u20ac';
						assert.equal(currency.format(1234.56, {currency: 'EUR', locale: 'de-de'}), expected);
					}
				}
			}
		},

		'.parse': {
			'en-us locale': {
				'USD currency': {
					'hundreds': function () {
						assert.equal(currency.parse('$123.45', {currency: 'USD', locale: 'en-us'}), 123.45);
					},

					'thousands': function () {
						assert.equal(currency.parse('$1,234.56', {currency: 'USD', locale: 'en-us'}), 1234.56);
					},

					'no cents': {
						'default use case': function () {
							assert.equal(currency.parse('$1,234', {currency: 'USD', locale: 'en-us'}), 1234);
						},

						'fractional false': function () {
							var options = {currency: 'USD', fractional: false, locale: 'en-us'};
							assert.equal(currency.parse('$1,234', options), 1234);
						},

						'fractional true - fails': function () {
							var options = {currency: 'USD', fractional: true, locale: 'en-us'};
							assert.isTrue(isNaN(currency.parse('$1,234', options)));
						}
					}
				}
			},

			'de-de locale': {
				'EUR currency': {
					'hundreds': function () {
						assert.equal(currency.parse('123,45 \u20ac', {currency: 'EUR', locale: 'de-de'}), 123.45);
						assert.equal(currency.parse('123,45\xa0\u20ac', {currency: 'EUR', locale: 'de-de'}), 123.45);
					},

					'thousands': function () {
						assert.equal(currency.parse('1.234,56 \u20ac', {currency: 'EUR', locale: 'de-de'}), 1234.56);
						assert.equal(currency.parse('1.234,56\u20ac', {currency: 'EUR', locale: 'de-de'}), 1234.56);
					}
				}
			}
		}
	});
});
