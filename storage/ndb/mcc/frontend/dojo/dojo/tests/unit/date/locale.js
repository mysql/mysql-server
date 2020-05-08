define([
	'intern!object',
	'intern/chai!assert',
	'../../../date/locale',
	'dojo/_base/kernel',
	'dojo/_base/array',
	'dojo/i18n',
	'dojo/Deferred'
], function (registerSuite, assert, locale, kernel, array, i18n, Deferred) {
	/* jshint maxlen: false */

	function assertDates(date1, date2) {
		assert.ok(date1, 'Date 1 does not have a value');
		assert.ok(date2, 'Date 2 does not have a value.');
		assert.instanceOf(date1, Date);
		assert.instanceOf(date2, Date);
		assert.equal(date1.getTime(), date2.getTime());
	}

	registerSuite({
		name: 'dojo/date/locale',

		setup: (function () {
			function loadAsync(partLocaleList) {
				var dfd = new Deferred();
				var deps = array.map(partLocaleList, function (locale) {
					return i18n.getL10nName('dojo/cldr', 'gregorian', locale);
				});
				require(deps, dfd.resolve);
				return dfd;
			}

			function loadSync(partLocaleList) {
				array.forEach(partLocaleList, function (locale) {
					/* global dojo */
					dojo.requireLocalization('dojo.cldr', 'gregorian', locale);
				});
			}

			return function () {
				var partLocaleList = ['en-us', 'fr-fr', 'es', 'de-at', 'ja-jp', 'zh-cn'];
				if (kernel.isAsync) {
					return loadAsync(partLocaleList);
				} else { // tests for the v1.x loader/i18n machinery
					loadSync(partLocaleList);
				}
			};
		})(),

		'.isWeekend': function () {
			var thursday = new Date(2006, 8, 21);
			var friday = new Date(2006, 8, 22);
			var saturday = new Date(2006, 8, 23);
			var sunday = new Date(2006, 8, 24);
			var monday = new Date(2006, 8, 25);

			assert.isFalse(locale.isWeekend(thursday, 'en-us'));
			assert.isFalse(locale.isWeekend(friday, 'en-us'));
			assert.isTrue(locale.isWeekend(saturday, 'en-us'));
			assert.isTrue(locale.isWeekend(sunday, 'en-us'));
			assert.isFalse(locale.isWeekend(monday, 'en-us'));
		},

		'.format': (function () {
			var date = new Date(2006, 7, 11, 0, 55, 12, 345);

			return {
				'full': function () {
					assert.equal(locale.format(date, {formatLength: 'full', selector: 'date', locale: 'en-us'}), 'Friday, August 11, 2006');
					assert.equal(locale.format(date, {formatLength: 'full', selector: 'date', locale: 'fr-fr'}), 'vendredi 11 ao\xFBt 2006');
					assert.equal(locale.format(date, {formatLength: 'full', selector: 'date', locale: 'de-at'}), 'Freitag, 11. August 2006');
					assert.equal(locale.format(date, {formatLength: 'full', selector: 'date', locale: 'ja-jp'}), '2006\u5E748\u670811\u65E5\u91D1\u66DC\u65E5');
				},

				'short': function () {
					assert.equal(locale.format(date, {formatLength: 'short', selector: 'date', locale: 'en-us'}), '8/11/06');
					assert.equal(locale.format(date, {formatLength: 'short', selector: 'date', locale: 'fr-fr'}), '11/08/2006');
					assert.equal(locale.format(date, {formatLength: 'short', selector: 'date', locale: 'de-at'}), '11.08.06');
					assert.equal(locale.format(date, {formatLength: 'short', selector: 'date', locale: 'ja-jp'}), '2006\u5E748\u670811\u65E5');
				},

				'time': function () {
					assert.equal(locale.format(date, {datePattern: 'E', selector: 'date'}), '6');
					assert.equal(locale.format(date, {selector: 'date', datePattern: 'EEE, MMM d, yyyy G', locale:'en-us' }), 'Fri, Aug 11, 2006 AD');
					assert.equal(locale.format(date, {formatLength: 'short', selector: 'time', locale: 'en-us'}), '12:55 AM');
					assert.equal(locale.format(date, {timePattern: 'h:m:s', selector: 'time'}), '12:55:12');
					assert.equal(locale.format(date, {timePattern: 'h:m:s.SS', selector: 'time'}), '12:55:12.35');
					assert.equal(locale.format(date, {timePattern: 'k:m:s.SS', selector: 'time'}), '24:55:12.35');
					assert.equal(locale.format(date, {timePattern: 'H:m:s.SS', selector: 'time'}), '0:55:12.35');
					assert.equal(locale.format(date, {timePattern: 'K:m:s.SS', selector: 'time'}), '0:55:12.35');
					assert.equal(locale.format(date, {formatLength: 'full', selector: 'time', locale: 'zh-cn'}).replace(/^.*(\u4e0a\u5348.*)/, '$1'), '\u4e0a\u534812:55:12');
				},

				'date': function () {
					var options = {datePattern: 'ddMMyyyy', selector: 'date'};
					assert.equal(locale.format(date, options), '11082006');

					options = {datePattern: 'hh \'o\'\'clock\' a', selector: 'date', locale: 'en'};
					assert.equal(locale.format(date, options), '12 o\'clock AM');

					options = {datePattern: 'dd/MM/yyyy', timePattern: 'hh:mma', locale: 'en', am: 'am', pm: 'pm'};
					assert.equal(locale.format(date, options), '11/08/2006, 12:55am');
				}
			};
		}()),

		'.parse': (function () {
			var AUG_11_2006 = new Date(2006, 7, 11, 0);
			var AUG_11_2006_12_30_AM = new Date(2006, 7, 11, 0, 30);
			var AUG_11_2006_12_30_PM = new Date(2006, 7, 11, 12, 30);

			return {
				// in the en locale the date format is M/d/yy
				// the time format is M/d/yy h:mm a
				'short format': {
					'leading zero month': function () {
						assertDates(locale.parse('08/11/06', {formatLength: 'short', selector: 'date', locale: 'en'}), AUG_11_2006);
					},

					'single digit month': function () {
						assertDates(locale.parse('8/11/06', {formatLength: 'short', selector: 'date', locale: 'en'}), AUG_11_2006);
					},

					'tolerate four digit year': function () {
						assertDates(locale.parse('8/11/2006', {formatLength: 'short', selector: 'date', locale: 'en'}), AUG_11_2006);
					},

					'four digit year in strict mode returns null': function () {
						assert.isNull(locale.parse('8/11/2006', {formatLength: 'short', selector: 'date', locale: 'en', strict: true}));
					},

					'upper case PM in time': function () {
						assertDates(locale.parse('08/11/06, 12:30 PM', {formatLength: 'short', locale: 'en'}, 'PM'), AUG_11_2006_12_30_PM);
					},

					'lower case pm in time': function () {
						assertDates(locale.parse('08/11/06, 12:30 pm', {formatLength: 'short', locale: 'en'}, 'pm'), AUG_11_2006_12_30_PM);
					},

					'lower case pm fails in strict mode': function () {
						assert.isNull(locale.parse('8/11/06, 12:30 pm', {formatLength: 'short', locale: 'en', strict: true}));
					},

					'upper case PM passes in strict mode': function () {
						assert.ok(locale.parse('8/11/06, 12:30 PM', {formatLength: 'short', locale: 'en', strict: true}));
					},

					'upper case AM in time': function () {
						assertDates(locale.parse('08/11/06, 12:30 AM', {formatLength: 'short', locale: 'en'}, 'AM'), AUG_11_2006_12_30_AM);
					}
				},

				// in the en locale the format is MMM d, yyyy
				'medium format': {
					'basic usage': function () {
						assertDates(locale.parse('Aug 11, 2006', {formatLength: 'medium', selector: 'date', locale: 'en'}), AUG_11_2006);
					},

					'tolerate peroid in month abbreviation': function () {
						assertDates(locale.parse('Aug. 11, 2006', {formatLength: 'medium', selector: 'date', locale: 'en'}), AUG_11_2006);
					},

					'period in month abbreviation returns null in strict mode': function () {
						assert.isNull(locale.parse('Aug. 11, 2006', {formatLength: 'medium', selector: 'date', locale: 'en', strict: true}));
					}
				},

				// in the en locale the format is MMMM d, yyyy
				'long format': {
					'basic usage': function () {
						assertDates(locale.parse('August 11, 2006', {formatLength: 'long', selector: 'date', locale: 'en'}), AUG_11_2006);
					}
				},

				// in the en locale the format is EEEE, MMMM d, yyyy
				'full format': {
					'basic usage': function () {
						assertDates(locale.parse('Friday, August 11, 2006', {formatLength: 'full', selector: 'date', locale: 'en'}), AUG_11_2006);
					}
				},

				'date pattern': {
					'ddMMyyyy': function () {
						assertDates(locale.parse('11082006', {datePattern: 'ddMMyyyy', selector: 'date'}), new Date(2006, 7, 11));
					},

					'ddMMMyyyy': function () {
						assertDates(locale.parse('31Aug2006', {datePattern: 'ddMMMyyyy', selector: 'date', locale: 'en'}), new Date(2006, 7, 31));
					},

					'DDD': function () {
						assertDates(locale.parse('007', {datePattern: 'DDD', selector: 'date'}), new Date(1970, 0, 7));
						assertDates(locale.parse('031', {datePattern: 'DDD', selector: 'date'}), new Date(1970, 0, 31));
						assertDates(locale.parse('100', {datePattern: 'DDD', selector: 'date'}), new Date(1970, 3, 10));
					}
				},

				'de locale': function () {
					assertDates(locale.parse('11.08.06', {formatLength: 'short', selector: 'date', locale: 'de'}), AUG_11_2006);
					assert.isNull(locale.parse('11.8/06', {formatLength: 'short', selector: 'date', locale: 'de'}));
					assert.isNull(locale.parse('11.8x06', {formatLength: 'short', selector: 'date', locale: 'de'}));
					assert.isNull(locale.parse('11.0.06', {formatLength: 'short', selector: 'date', locale: 'de'}));
					assert.isNull(locale.parse('11.13.06', {formatLength: 'short', selector: 'date', locale: 'de'}));
					assert.isNull(locale.parse('32.08.06', {formatLength: 'short', selector: 'date', locale: 'de'}));
				},

				'es locale': {
					'short format': {
						'leading zero month': function () {
							assertDates(locale.parse('11/08/06', {formatLength: 'short', selector: 'date', locale: 'es'}), AUG_11_2006);
						},

						'single digit month': function () {
							assertDates(locale.parse('11/8/06', {formatLength: 'short', selector: 'date', locale: 'es'}), AUG_11_2006);
						},

						'tolerate four digit year': function () {
							assertDates(locale.parse('11/8/2006', {formatLength: 'short', selector: 'date', locale: 'es'}), AUG_11_2006);
						},

						'four digit year in strict mode returns null': function () {
							assert.isNull(locale.parse('11/8/2006', {formatLength: 'short', selector: 'date', locale: 'es', strict: true}));
						}
					},

					'long format': {
						'basic usage': function () {
							assertDates(locale.parse('11 de agosto de 2006', {formatLength: 'long', selector: 'date', locale: 'es'}), AUG_11_2006);
						},

						'case-insensitive month': function () {
							assertDates(locale.parse('11 de Agosto de 2006', {formatLength: 'long', selector: 'date', locale: 'es'}), AUG_11_2006);
						},

						'case-insensitive month fails in strict mode': function () {
							assert.isNull(locale.parse('Viernes, 11 de agosto de 2006', {formatLength: 'full', selector: 'date', locale: 'es', strict: true}));
						}
					}
				},

				//Japanese (ja)
				//note: to avoid garbling from non-utf8-aware editors that may touch this file, using the \uNNNN format
				//for expressing double-byte chars.
				//toshi (year): \u5e74
				//getsu (month): \u6708
				//nichi (day): \u65e5
				//kinyoubi (Friday): \u91d1\u66dc\u65e5
				//zenkaku space: \u3000
				'ja locale': {
					'short format': {
						'leading zero month': function () {
							assertDates(locale.parse('06\u5E7408\u670811\u65E5', {formatLength: 'short', selector: 'date', locale: 'ja'}), AUG_11_2006);
						},

						'single digit month': function () {
							assertDates(locale.parse('06\u5E748\u670811\u65E5', {formatLength: 'short', selector: 'date', locale: 'ja'}), AUG_11_2006);
						},

						'tolerate four digit year': function () {
							assertDates(locale.parse('2006\u5E748\u670811\u65E5', {formatLength: 'short', selector: 'date', locale: 'ja'}), AUG_11_2006);
						}

						// 'four digit year in strict mode returns null': function () {
						// 	assert.isNull(locale.parse('2006\u5E748\u670811\u65E5', {formatLength: 'short', selector: 'date', locale: 'ja', strict: true}));
						// }
					},

					'medium format': {
						'leading zero month': function () {
							assertDates(locale.parse('2006\u5E7408\u670811\u65E5', {formatLength: 'medium', selector: 'date', locale: 'ja'}), AUG_11_2006);
						},

						'single digit month': function () {
							assertDates(locale.parse('2006\u5E748\u670811\u65E5', {formatLength: 'medium', selector: 'date', locale: 'ja'}), AUG_11_2006);
						}
					},

					'long format': {
						'basic usage': function () {
							assertDates(locale.parse('2006\u5e748\u670811\u65e5', {formatLength: 'long', selector: 'date', locale: 'ja'}), AUG_11_2006);
						}
					},

					'full format': {
						'basic usage': function () {
							assertDates(locale.parse('2006\u5e748\u670811\u65e5\u91d1\u66dc\u65e5', {formatLength: 'full', selector: 'date', locale: 'ja'}), AUG_11_2006);
						}
					}
				},

				'fr-fr locale': {
					'medium format': {
						'tolerance for abbreviations': function () {
							assertDates(locale.parse('11 avr 06', { formatLength: 'medium', selector: 'date', locale: 'fr-fr' }), new Date(2006, 3, 11, 0));
						},

						'round trip': function () {
							var options = { formatLength: 'medium', selector: 'date', locale: 'fr-fr' };
							assertDates(locale.parse(locale.format(AUG_11_2006, options), options), AUG_11_2006);
						}
					}
				},

				'he locale': {
					'basic usage via roundtrip': function () {
						var options = {locale: 'he', formatLength: 'full', selector: 'date'};
						assert.ok(locale.parse(locale.format(new Date(), options), options));
					}
				},

				'times': function () {
					var time = new Date(2006, 7, 11, 12, 30);
					var tformat = {selector: 'time', strict: true, timePattern: 'h:mm a', locale: 'en'};

					assert.equal(locale.parse('12:30 PM', tformat).getHours(), time.getHours());
					assert.equal(locale.parse('12:30 PM', tformat).getMinutes(), time.getMinutes());
				},

				'format patterns': function () {
					var time = new Date(2006, 7, 11, 12, 30);
					var tformat;

					tformat = {selector: 'time', strict: true, timePattern: 'h \'o\'\'clock\'', locale: 'en'};
					assert.equal(locale.parse('12 o\'clock', tformat).getHours(), time.getHours());

					tformat = {selector: 'time', strict: true, timePattern: ' \'Hour is\' h', locale: 'en'};
					assert.equal(locale.parse(' Hour is 12', tformat).getHours(), time.getHours());

					tformat = {selector: 'time', strict: true, timePattern: '\'Hour is\' h', locale: 'en'};
					assert.equal(locale.parse('Hour is 12', tformat).getHours(), time.getHours());
				},

				'invalid dates fail': function () {
					assert.isNull(locale.parse('2/29/2007', {formatLength: 'short', selector: 'date', locale: 'en'}));
					assert.isNull(locale.parse('4/31/2007', {formatLength: 'short', selector: 'date', locale: 'en'}));
					assert.isNull(locale.parse('Decemb 30, 2007', {formatLength: 'long', selector: 'date', locale: 'en'}));
				},

				'month > 12 fails': function () {
					assert.isNull(locale.parse('15/1/2005', {formatLength: 'short', selector: 'date', locale: 'en'}));
				},

				'day of month > number days in month fails': function () {
					assert.isNull(locale.parse('Aug 32, 2006', {formatLength: 'medium', selector: 'date', locale: 'en'}));
				},

				'pre-epoch date': function () {
					var AUG_11_06CE = new Date(2006, 7, 11, 0);

					AUG_11_06CE.setFullYear(6); // literally the year 6 C.E.
					assertDates(locale.parse('Aug 11, 06', {selector: 'date', datePattern: 'MMM dd, yyyy', locale: 'en', strict: true}), AUG_11_06CE);
				},

				'dates with no spaces': function () {
					assertDates(locale.parse('11Aug2006', {selector: 'date', datePattern: 'ddMMMyyyy', locale: 'en'}), AUG_11_2006);
					assertDates(locale.parse('Aug2006', {selector: 'date', datePattern: 'MMMyyyy', locale: 'en'}), new Date(2006, 7, 1));
					assertDates(locale.parse('111910', {fullyear: false, datePattern: 'MMddyy', selector: 'date'}), new Date(2010, 10, 19));
				}
			};
		})(),

		'._getDayOfYear': function () {
			assert.equal(locale._getDayOfYear(new Date(2006, 0, 1)), 1);
			assert.equal(locale._getDayOfYear(new Date(2006, 1, 1)), 32);
			assert.equal(locale._getDayOfYear(new Date(2007, 2, 13, 0, 13)), 72);
			assert.equal(locale._getDayOfYear(new Date(2007, 2, 13, 1, 13)), 72);
		},

		'._getWeekOfYear': function () {
			assert.equal(locale._getWeekOfYear(new Date(2000, 0, 1)), 0);
			assert.equal(locale._getWeekOfYear(new Date(2000, 0, 2)), 1);
			assert.equal(locale._getWeekOfYear(new Date(2000, 0, 2), 1), 0);
			assert.equal(locale._getWeekOfYear(new Date(2007, 0, 1)), 0);
			assert.equal(locale._getWeekOfYear(new Date(2007, 0, 1), 1), 1);
			assert.equal(locale._getWeekOfYear(new Date(2007, 6, 14)), 27);
			assert.equal(locale._getWeekOfYear(new Date(2007, 6, 14), 1), 28);
		}
	});
});
