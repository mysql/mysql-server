define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'./support/loaderTest',
	'./support/pageReady'
], function (require, registerSuite, assert, loaderTest, pageReady) {
	function syncAsyncTests(url, exec) {
		url = require.toUrl(url);
		var sep = url.indexOf('?') > -1 ? '&' : '?';
		return {
			'sync': pageReady(url, exec),
			'async': pageReady(url + sep + 'async', exec)
		};
	}
	registerSuite({
		name: 'dojo/_base/loader - requirejs',

		simple: syncAsyncTests(
			'./requirejs/simple.html',
			function (command) {
				return command.executeAsync(function (callback) {
					require({
						baseUrl: './'
					}, [
						'map', 'simple', 'dimple', 'func'
					], function (map, simple, dimple, func) {
						callback(JSON.stringify({
							mapName: map.name,
							simpleColor: simple.color,
							dimpleColor: dimple.color,
							funcOut: func()
						}));
					});
				}).then(function (data) {
					data = JSON.parse(data);
					assert.strictEqual(data.mapName, 'map');
					assert.strictEqual(data.simpleColor, 'blue');
					assert.strictEqual(data.dimpleColor, 'dimple-blue');
					assert.strictEqual(data.funcOut, 'You called a function');
				}).executeAsync(function (callback) {
					var path = this.location.href.replace(/simple\.html.*$/, 'foo');
					var index = path.indexOf(':');
					var noProtocolPath = path.substring(index + 1, path.length).replace(/foo/, 'bar');
					var self = this;

					require([ path, noProtocolPath ], function () {
						callback(JSON.stringify({
							fooName: self.foo.name,
							barName: self.bar.name
						}));
					});
				}).then(function (data) {
					data = JSON.parse(data);
					assert.strictEqual(data.fooName, 'foo');
					assert.strictEqual(data.barName, 'bar');
				});
			}
		),

		config: syncAsyncTests(
			'./requirejs/config.html',
			function (command) {
				return command.executeAsync(function (callback) {
					callback(JSON.stringify(window.testData));
				}).then(function (data) {
					data = JSON.parse(data);
					assert.strictEqual(data.simpleColor, 'blue');
					assert.strictEqual(data.dimpleColor, 'dimple-blue');
					assert.strictEqual(data.funcOut, 'You called a function');
				});
			}
		),

		'simple, no head': syncAsyncTests(
			'./requirejs/simple-nohead.html',
			function (command) {
				return command.executeAsync(function (callback) {
					require([ 'simple', 'dimple', 'func'], function (simple, dimple, func) {
						callback(JSON.stringify({
							simpleColor: simple.color,
							dimpleColor: dimple.color,
							funcOut: func()
						}));
					});
				}).then(function (data) {
					data = JSON.parse(data);
					assert.strictEqual(data.simpleColor, 'blue');
					assert.strictEqual(data.dimpleColor, 'dimple-blue');
					assert.strictEqual(data.funcOut, 'You called a function');
				});
			}
		),

		circular: pageReady(
			require.toUrl('./requirejs/circular.html?async'),
			function (command) {
				return command.executeAsync(function (callback) {
					require(['require', 'two', 'funcTwo', 'funcThree'],
						function (require, two, FuncTwo, funcThree) {
							var args = two.doSomething();
							var twoInst = new FuncTwo('TWO');
							callback(JSON.stringify({
								size: args.size,
								color: args.color,
								name: twoInst.name,
								oneName: twoInst.oneName(),
								three: funcThree('THREE')
							}));
						});
				}).then(function (data) {
					data = JSON.parse(data);
					assert.strictEqual(data.size, 'small');
					assert.strictEqual(data.color, 'redtwo');
					assert.strictEqual(data.name, 'TWO');
					assert.strictEqual(data.oneName, 'ONE-NESTED');
					assert.strictEqual(data.three, 'THREE-THREE_SUFFIX');
				});
			}
		),

		'url fetch': syncAsyncTests(
			'./requirejs/urlfetch/urlfetch.html',
			function (command) {
				return command.executeAsync(function (callback) {
					require({
						baseUrl: './',
						paths: {
							'one': 'two',
							'two': 'two',
							'three': 'three',
							'four': 'three'
						}
					}, [ 'one', 'two', 'three', 'four' ], function (one, two, three, four) {
						var scripts = document.getElementsByTagName('script');
						var counts = {};
						var url;

						/* First confirm there is only one script tag for each module */
						for (var i = scripts.length - 1; i > -1; i--) {
							url = scripts[i].src;
							if (url) {
								if (!(url in counts)) {
									counts[url] = 0;
								}
								counts[url] += 1;
							}
						}

						var data = {
							oneName: one.name,
							twoOneName: two.oneName,
							twoName: two.name,
							threeName: three.name,
							fourThreeName: four.threeName,
							fourName: four.name
						};
						if (require.async) {
							data.counts = [];
							for (var prop in counts) {
								data.counts.push(counts[prop]);
							}
						}
						callback(JSON.stringify(data));
					});
				}).then(function (data) {
					data = JSON.parse(data);

					if (data.counts) {
						var count;
						while ((count = data.counts.shift()) !== undefined) {
							assert.strictEqual(count, 1);
						}
					}
					assert.strictEqual(data.oneName, 'one');
					assert.strictEqual(data.twoOneName, 'one');
					assert.strictEqual(data.twoName, 'two');
					assert.strictEqual(data.threeName, 'three');
					assert.strictEqual(data.fourThreeName, 'three');
					assert.strictEqual(data.fourName, 'four');
				});
			}
		),

		dataMain: syncAsyncTests(
			'./requirejs/dataMain.html',
			function (command) {
				return command.execute(function () {
					return this.simple.color;
				}).then(function (data) {
					assert.strictEqual(data, 'blue');
				});
			}
		),

		depoverlap: syncAsyncTests(
			'./requirejs/depoverlap.html',
			function (command) {
				return command.execute(function () {
					//First confirm there is only one script tag for each module:
					var scripts = this.document.getElementsByTagName('script');
					var i;
					var counts = {};
					var modName;
					var something;

					for (i = scripts.length - 1; i > -1; i--) {
						modName = scripts[i].getAttribute('data-requiremodule');
						if (modName) {
							if (!(modName in counts)) {
								counts[modName] = 0;
							}
							counts[modName] += 1;
						}
					}

					something = this.uno.doSomething();

					return {
						counts: counts,
						unoName: this.uno.name,
						dosName: something.dosName,
						tresName: something.tresName
					};
				}).then(function (data) {
					//Now that we counted all the modules make sure count
					//is always one.
					var counts = data.counts;
					for (var prop in counts) {
						assert.strictEqual(counts[prop], 1);
					}

					assert.strictEqual(data.unoName, 'uno');
					assert.strictEqual(data.dosName, 'dos');
					assert.strictEqual(data.tresName, 'tres');
				});
			}
		),
		// TODO: there are more of the i18n tests...
		'i18n': {
			'i18n': (function () {
				function i18nTest(command) {
					return command.executeAsync(function (callback) {
						//Allow locale to be set via query args.
						var locale = null;
						var query = this.location.href.split('#')[0].split('?')[1];
						var match = query && query.split('&')[0].match(/locale=([\w-]+)/);
						if (match) {
							locale = match[1];
						}

						//Allow bundle name to be loaded via query args.
						var bundle = 'i18n!nls/colors';
						match = query && query.match(/bundle=([^\&]+)/);
						if (match) {
							bundle = match[1];
						}

						var red = 'red';
						var blue = 'blue';
						var green = 'green';

						if (locale && locale.indexOf('en-us-surfer') !== -1 || bundle.indexOf('nls/en-us-surfer/colors') !== -1) {
							red = 'red, dude';
						}
						else if ((locale && locale.indexOf('fr-') !== -1) || bundle.indexOf('fr-') !== -1) {
							red = 'rouge';
							blue = 'bleu';
						}

						require([ 'dojo' ], function (dojo) {
							// dojo/i18n! looks at dojo.locale
							locale && (dojo.locale = locale);
							require([ bundle ], function (colors) {
								callback({
									red: {actual: colors.red, expected: red},
									blue: {actual: colors.blue, expected: blue},
									green: {actual: colors.green, expected: green}
								});
							});
						});
					}).then(function (data) {
						assert.strictEqual(data.red.actual, data.red.expected);
						assert.strictEqual(data.blue.actual, data.blue.expected);
						assert.strictEqual(data.green.actual, data.green.expected);
					});
				}

				return {
					'locale unknown': syncAsyncTests('./requirejs/i18n/i18n.html?bundle=i18n!nls/fr-fr/colors', i18nTest),
					'base': syncAsyncTests('./requirejs/i18n/i18n.html', i18nTest),
					'locale': syncAsyncTests('./requirejs/i18n/i18n.html?locale=en-us-surfer', i18nTest),
					'bundle': syncAsyncTests('./requirejs/i18n/i18n.html?bundle=i18n!nls/en-us-surfer/colors', i18nTest)
				};
			})(),

			common: (function () {
				function commonTest(command) {
					return command.executeAsync(function (callback) {
						//Allow locale to be set via query args.
						var locale = null;
						var query = this.location.href.split('#')[0].split('?')[1];
						var match = query && query.match(/locale=([\w-]+)/);

						if (match) {
							locale = match[1];
						}

						var red = 'red';
						var blue = 'blue';

						if (locale && locale.indexOf('en-us-surfer') !== -1) {
							red = 'red, dude';
						}
						else if ((locale && locale.indexOf('fr-') !== -1)) {
							red = 'rouge';
							blue = 'bleu';
						}

						require([ 'dojo' ], function (dojo) {
							// dojo/i18n! looks at dojo.locale
							locale && (dojo.locale = locale);
							require([ 'commonA', 'commonB' ], function (commonA, commonB) {
								callback({
									commonA: {actual: commonA, expected: red},
									commonB: {actual: commonB, expected: blue}
								});
							});
						});
					}).then(function (data) {
						assert.strictEqual(data.commonA.actual, data.commonA.expected);
						assert.strictEqual(data.commonB.actual, data.commonB.expected);
					});
				}

				return {
					base: syncAsyncTests('./requirejs/i18n/i18n.html', commonTest),
					locale: syncAsyncTests('./requirejs/i18n/i18n.html?locale=en-us-surfer', commonTest)
				};
			})()
		},

		paths: syncAsyncTests('./requirejs/paths/paths.html', function (command) {
			return command.executeAsync(function (callback) {
				var scriptCounter = 0;
				var self = this;

				require({
					baseUrl: './',
					packages: [
						{
							name: 'first',
							location: 'first.js',
							main: './first'
						}
					]
				}, [ 'first!whatever' ], function (first) {
					//First confirm there is only one script tag for each
					//module:
					var scripts = self.document.getElementsByTagName('script');
					var modName;

					for (var i = scripts.length - 1; i > -1; i--) {
						modName = scripts[i].getAttribute('src');
						if (/first\.js$/.test(modName)) {
							scriptCounter += 1;
						}
					}

					var result = {
						async: require.async,
						globalCounter: self.globalCounter,
						name: first.name,
						secondName: first.secondName

					};
					if (require.async) {
						result.scriptCounter = scriptCounter;
					}
					callback(result);
				});
			}).then(function (data) {
				if (data.async) {
					assert.strictEqual(data.scriptCounter, 1);
				}

				assert.strictEqual(data.globalCounter, 2);
				assert.strictEqual(data.name, 'first');
				assert.strictEqual(data.secondName, 'second');
			});
		}),

		relative: syncAsyncTests('./requirejs/relative/relative.html', function (command) {
			return command.executeAsync(function (callback) {
				// alias dojo's text module to text!
				define('text', [ 'dojo/text' ], function (text) {
					return text;
				});

				require({
					baseUrl: require.has('host-browser') ? './' : './relative/',
					paths: {
						text: '../../text'
					}
				}, [ 'foo/bar/one' ], function (one) {
					callback({
						name: one.name,
						twoName: one.twoName,
						threeName: one.threeName,
						message: one.message.replace(/\r|\n/g, '')
					});
				});
			}).then(function (data) {
				assert.strictEqual(data.name, 'one');
				assert.strictEqual(data.twoName, 'two');
				assert.strictEqual(data.threeName, 'three');
				assert.strictEqual(data.message, 'hello world');
			});
		}),

		text: (function () {
			function textTest(command, useAlias) {
				return command.executeAsync(function (useAlias, callback) {
					if (useAlias) {
						// alias dojo's text module to text!
						require({ aliases: [
							[ 'text', 'dojo/text' ]
						] });
					}
					else {
						define('text', [ 'dojo/text' ], function (text) {
							return text;
						});
					}

					require({
						baseUrl: './',
						paths: {
							text: '../../text'
						}
					}, [
						'widget',
						'local',
						'text!resources/sample.html!strip'
					], function (widget, local, sampleText) {
						var results = [];

						function makeResults(expected, actual) {
							results.push({
								actual: actual.replace(/\s{2,}|\n/g, ''),
								expected: expected
							});
						}

						makeResults('<span>Hello World!</span>', sampleText);
						makeResults('<div data-type="widget"><h1>This is a widget!</h1><p>I am in a widget</p></div>', widget.template);
						makeResults('subwidget', widget.subWidgetName);
						makeResults('<div data-type="subwidget"><h1>This is a subwidget</h1></div>', widget.subWidgetTemplate);
						makeResults('<span>This! is template2</span>', widget.subWidgetTemplate2);
						makeResults('<h1>Local</h1>', local.localHtml);
						callback(results);
					});
				}, [useAlias]).then(function (data) {
					var test;
					while ((test = data.shift())) {
						assert.strictEqual(test.actual, test.expected);
					}
				});
			}

			return {
				alias: syncAsyncTests('./requirejs/text/text.html', function (command) {
					return textTest(command, true);
				}),
				'non-alias': syncAsyncTests('./requirejs/text/text.html', textTest)
			};
		})(),

		'text only': syncAsyncTests('./requirejs/text/textOnly.html', function (command) {
			return command.executeAsync(function (callback) {
				// alias dojo's text module to text!
				define('text', [ 'dojo/text' ], function (text) {
					return text;
				});

				require({
					baseUrl: './',
					paths: {
						text: '../../text'
					}
				}, [ 'text!resources/sample.html!strip'], function (sampleText) {
					callback(sampleText);
				});
			}).then(function (data) {
				assert.strictEqual(data, '<span>Hello World!</span>');
			});
		}),

		exports: pageReady(
			require.toUrl('./requirejs/exports/exports.html'),
			function (command) {
				return command.executeAsync(function (callback) {
					require({
						baseUrl: require.has('host-browser') ? './' : './exports/'
					}, [
						'vanilla',
						'funcSet',
						'assign',
						'assign2',
						'usethis',
						'implicitModule',
						'simpleReturn'
					], function (vanilla, funcSet, assign, assign2, usethis, implicitModule, simpleReturn) {
						callback({
							vanillaName: vanilla.name,
							funcSet: funcSet,
							assign: assign,
							assign2: assign2,
							implicitModule: implicitModule(),
							simpleReturn: simpleReturn()
						});
					});
				}).then(function (data) {
					assert.strictEqual(data.vanillaName, 'vanilla');
					assert.strictEqual(data.funcSet, 'funcSet');
					assert.strictEqual(data.assign, 'assign');
					assert.strictEqual(data.assign2, 'assign2');
					assert.strictEqual(data.implicitModule, 'implicitModule');
					assert.strictEqual(data.simpleReturn, 'simpleReturn');
				});
			}
		),
		uniques: syncAsyncTests('./requirejs/uniques/uniques.html', function (command) {
			return command.executeAsync(function (callback) {
				require({
						baseUrl: './'
				}, [ 'one', 'two', 'three' ], function (one, two, three) {
					callback({
						oneName: one.name,
						oneThreeName: one.threeName,
						oneThreeName2: one.threeName2,
						twoOneName: two.oneName,
						twoOneName2: two.oneName2,
						twoName: two.name,
						twoThreeName: two.threeName,
						threeName: three.name
					});
				});
			}).then(function (data) {
				assert.strictEqual(data.oneName, 'one');
				assert.strictEqual(data.oneThreeName, 'three');
				assert.strictEqual(data.oneThreeName2, 'three');
				assert.strictEqual(data.twoOneName, 'one');
				assert.strictEqual(data.twoOneName2, 'one');
				assert.strictEqual(data.twoName, 'two');
				assert.strictEqual(data.twoThreeName, 'three');
				assert.strictEqual(data.threeName, 'three');
			});
		})/*,
		TODO: Fix these tests
		'simple, bad base': (function () {
			function badBaseTest(command) {
				return command.executeAsync(function (callback) {
					// set the base URL
					require({ baseUrl: window.testBase + '/loader/requirejs/' });

					require([ 'simple', 'dimple', 'func' ], function (simple, dimple, func) {
						callback({
							simple: simple.color,
							dimple: dimple.color,
							func: func()
						});
					});
				}).then(function (data) {
					assert.strictEqual(data.simple, 'blue');
					assert.strictEqual(data.dimple, 'dimple-blue');
					assert.strictEqual(data.func, 'You called a function');

					return this.session.executeAsync(function (callback) {
						//This test is only in the HTML since it uses an URL for a require
						//argument. It will not work well in say, the Rhino tests.
						var path = location.href.replace(/simple-badbase\.html.*$/, 'foo');
						var index = path.indexOf(':');
						var noProtocolPath = path.substring(index + 1, path.length).replace(/foo/, 'bar');

						require([ path, noProtocolPath ], function () {
							callback({
								foo: window.foo.name,
								bar: window.bar.name
							});
						});
					}).then(function (data) {
						assert.strictEqual(data.foo, 'foo');
						assert.strictEqual(data.bar, 'bar');
					});
				});
			}

			return {
				sync: pageReady(require.toUrl('./requirejs/simple-badbase.html'), badBaseTest),
				async: pageReady(require.toUrl('./requirejs/simple-badbase.html?async'), function (command) {
					return command.executeAsync(function (callback) {
						require([ 'dojo/sniff' ], function (has) {
							callback(has('ie'));
						});
					}).then(function (ie) {
						if (!ie || ie > 6) {
							return badBaseTest(this.session);
						}
					});
				})
			};
		})()*/
	});
});
