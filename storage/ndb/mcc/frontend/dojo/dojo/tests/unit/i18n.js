define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../../i18n',
	'dojo/_base/array',
	'dojo/Deferred',
	'dojo/_base/lang'
], function (require, registerSuite, assert, i18n, array, Deferred, lang) {
	var bundlePath = require.toAbsMid('./support/i18n');

	function localePluginTest(locale, expected) {
		return function () {
			var dfd = new Deferred();
			var modulePath = i18n.getL10nName(bundlePath, 'salutations', locale);

			require([modulePath], function (bundle) {
				dfd.resolve(bundle);
			});

			return dfd.then(function (bundle) {
				assert.equal(bundle.hello, expected);
			});
		};
	}

	function getLocalizationTest(locale, expected) {
		return function() {
			var actual = i18n.getLocalization(bundlePath, 'salutations', locale).hello;
			assert.equal(actual, expected);
		};
	}

	function buildParameterizedTests(testData, testFn) {
		var tests = {};

		// Parameterized tests
		array.forEach(testData, function (parameters) {
			tests[parameters[0]] = testFn(parameters[1], parameters[2]);
		});

		return tests;
	}

	registerSuite(function () {
		var PARAMS = [
			['Locale which overrides root translation', 'de', 'Hallo'],
			['Locale which does not override root translation', 'en', 'Hello'],
			['Locale which overrides its parent', 'en-au', 'G\'day'],
			['Locale which does not override its parent', 'en-us', 'Hello'],
			['Locale which overrides its parent and ancestor', 'en-us-texas', 'Howdy'],
			['3rd level variant which overrides its parent', 'en-us-new_york', 'Hello'],
			['Locale which overrides its grandparent', 'en-us-new_york-brooklyn', 'Yo'],
			['Locale which does not have any translation available', 'xx', 'Hello'],
			['A double-byte string should be read in as UTF-8 and treated as unicode', 'zh-cn', '\u4f60\u597d']
		];

		return {
			name: 'dojo/i18n',
			
			'construction': function () {
				assert.isDefined(i18n.getLocalization);
			},

			'plugin .load()': buildParameterizedTests(PARAMS, localePluginTest),

			'.getLocalization': lang.mixin(buildParameterizedTests(PARAMS, getLocalizationTest), {
				'missing bundle throws': function () {
					assert.throws(function () {
						getLocalizationTest('lolipop-guild', undefined)();
					});
				},
				'cached results': function () {
					var l10n = i18n.getLocalization(bundlePath, 'salutations', 'en');
					assert.equal(l10n.hello, 'Hello');
					l10n.hello = 'test';
					l10n = i18n.getLocalization(bundlePath, 'salutations', 'en');
					assert.equal(l10n.hello, 'test');
				}
			})
		};
	});
});
