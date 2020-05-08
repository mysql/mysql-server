define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'./loader/support/loaderTest',
	'./loader/support/pageReady',
	'./loader/requirejs',
	'./loader/xdomain',
	'./loader/moduleIds',
	'./loader/hostenv_webworkers'
], function (require, registerSuite, assert, loaderTest, pageReady) {
	registerSuite({
		name: 'dojo/_base/loader',

		'async with dojo require': loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: true,
				baseUrl: '.',
				packages: [
					{ name: 'dijit', location: 'dijit' },
					{ name: 'dojox', location: 'dojox' },
					{ name: 'dojo', location: '../../../../node_modules/dojo' },
					{ name: 'test-modules', location: '.' }
				]
			},
			function (callback) {
				// dojo must be loaded for legacy modes to work
				require({ async: 'sync' }, [ 'dojo' ]);

				// now we can switch to legacy async mode
				require({ async: 'legacyAsync' });

				require([ 'test-modules/syncFromAsyncModule' ], function () {
					callback(window.syncFromAsyncModule);
				});
			},
			function (value) {
				assert.strictEqual(value, 'OK');
			}
		),

		'config': (function () {
			function createConfigTest(search, configName) {
				return loaderTest(
					require.toUrl('./loader/config.html') + '?' + search,
					[
						function (configName, callback) {
							require([
								'dojo',
								'dojo/has'
							], function (dojo, has) {
								var config = window[configName];
								var data = {
									baseUrl: require.baseUrl,
									config: config,
									requireConfig: require.rawConfig,
									dojoConfig: dojo.config
								};
								var hasResults = {};
								for (var key in config.has) {
									hasResults[key] = has(key);
								}
								data.has = hasResults;
								callback(data);
							});
						},
						[ configName ]
					],
					function (data) {
						var config = data.config;
						var requireConfig = data.requireConfig;
						var dojoConfig = data.dojoConfig;

						assert.strictEqual(requireConfig.baseUrl, config.baseUrl);
						assert.strictEqual(requireConfig.waitSeconds, config.waitSeconds);
						assert.strictEqual(requireConfig.locale, config.locale);
						assert.deepEqual(requireConfig.has, config.has);
						assert.strictEqual(requireConfig.cats, config.cats);
						assert.strictEqual(requireConfig.a, config.a);
						assert.deepEqual(requireConfig.b, config.b);
						assert.strictEqual(data.baseUrl, config.baseUrl + '/');
						for (var key in data.has) {
							assert.ok(data.has[key]);
						}
						assert.isUndefined(require.cats);
						assert.isUndefined(require.a);
						assert.isUndefined(require.b);

						assert.strictEqual(dojoConfig.baseUrl, config.baseUrl + '/');
						assert.strictEqual(dojoConfig.waitSeconds, config.waitSeconds);
						assert.strictEqual(dojoConfig.locale, config.locale);
						assert.strictEqual(dojoConfig.cats, config.cats);
						assert.strictEqual(dojoConfig.a, config.a);
						assert.deepEqual(dojoConfig.b, config.b);
					}
				);
			}

			var testMap = {
				'djConfig': '_djConfig',
				'djConfig-require': '_djConfig',
				'dojoConfig': '_dojoConfig',
				'dojoConfig-djConfig': '_dojoConfig',
				'dojoConfig-require': '_dojoConfig',
				'dojoConfig-djConfig-require': '_dojoConfig',
				'require': '_require'
			};

			var tests = {};

			for (var key in testMap) {
				tests[key] = createConfigTest(key, testMap[key]);
			}

			return tests;
		})(),

		'config api': loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: true,
				isDebug: true,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				require([ 'dojo' ], function (dojo) {
					dojo.ready(function () {
						var data = { async: {} };
						var runData;

						function listener1(config, rawConfig) {
							runData.called1 = true;
							runData.rawConfig1 = dojo.mixin({}, rawConfig);
						}
						function listener2(config, rawConfig) {
							runData.called2 = true;
							runData.rawConfig2 = dojo.mixin({}, rawConfig);
						}

						var handle1 = require.on('config', listener1);
						var handle2 = require.on('config', listener2);

						data.call1 = runData = {};
						require({
							someFeature: 1
						});

						handle1.remove();

						data.call2 = runData = {};
						require({
							someFeature: 0,
							someOtherFeature: 1
						});

						handle2.remove();

						function recordResults(key) {
							data.async[key] = {
								async: require.async,
								legacyMode: require.legacyMode
							};
						}

						require({ async: 1 });
						recordResults('1');

						require({ async: true });
						recordResults('true');

						require({ async: 2 });
						recordResults('2');

						require({ async: 'nonsense' });
						recordResults('nonsense');

						require({ async: 0 });
						recordResults('0');

						require({ async: false });
						recordResults('false');

						require({ async: 'sync' });
						recordResults('sync');

						require({ async: 'legacyAsync' });
						recordResults('legacyAsync');

						callback(data);
					});
				});
			},
			function (data) {
				var call1 = data.call1;
				assert.ok(call1.called1);
				assert.ok(call1.called2);
				assert.strictEqual(call1.rawConfig1.someFeature, 1);
				assert.strictEqual(call1.rawConfig2.someFeature, 1);

				var call2 = data.call2;
				assert.notOk(call2.called1);
				assert.ok(call2.called2);
				assert.strictEqual(call2.rawConfig2.someFeature, 0);
				assert.strictEqual(call2.rawConfig2.someOtherFeature, 1);

				var expected;
				for (var key in data.async) {
					expected = {
						async: false,
						legacyMode: false
					};
					if (key === 'legacyAsync') {
						expected.legacyMode = 'legacyAsync';
					}
					else if (key in { 0: 1, 'false': 1, sync: 1 }) {
						expected.legacyMode = 'sync';
					}
					else {
						expected.async = true;
					}
					assert.deepEqual(data.async[key], expected);
				}
			}
		),

		'config sniff': (function () {
			function createSniffTest(url) {
				return loaderTest(
					require.toUrl(url),
					function (callback) {
						require(['dojo'], function (dojo) {
							callback({
								requireConfig: require.rawConfig,
								dojoConfig: dojo.config,
								async: require.async,
								baseUrl: require.baseUrl,
								cats: require.cats === undefined,
								a: require.a === undefined,
								b: require.b === undefined
							});
						});
					},
					function (data) {
						var requireConfig = data.requireConfig;
						var dojoConfig = data.dojoConfig;

						assert.ok(requireConfig.async);
						assert.strictEqual(requireConfig.baseUrl, '../../../..');
						assert.strictEqual(requireConfig.waitSeconds, 6);
						assert.strictEqual(requireConfig.cats, 'dojo-config-dogs');
						assert.strictEqual(requireConfig.a, 2);
						assert.deepEqual(requireConfig.b, [ 3, 4, 5 ]);

						assert.ok(data.async);
						assert.strictEqual(data.baseUrl, '../../../../');
						assert.isTrue(data.cats);
						assert.isTrue(data.a);
						assert.isTrue(data.b);

						assert.strictEqual(dojoConfig.baseUrl, '../../../../');
						assert.strictEqual(dojoConfig.cats, 'dojo-config-dogs');
						assert.strictEqual(dojoConfig.a, 2);
						assert.deepEqual(dojoConfig.b, [ 3, 4, 5 ]);
					}
				);
			}

			return {
				dojoConfig: createSniffTest('./loader/config-sniff.html'),
				djConfig: createSniffTest('./loader/config-sniff-djConfig.html')
			};
		})(),

		'config has': pageReady(
			require.toUrl('./loader/config-has.html'),
			function (command) {
				return command.executeAsync(function (callback) {
					require([ 'dojo/has' ], function (has) {
						callback({
							requireConfig: require.rawConfig,
							has: {
								'config-someConfigSwitch': has('config-someConfigSwitch'),
								'config-isDebug': has('config-isDebug'),
								'config-anotherConfigSwitch': has('config-anotherConfigSwitch'),
								'some-has-feature': has('some-has-feature')
							}
						});
					});
				}).then(function (data) {
					var requireConfig = data.requireConfig;
					assert.strictEqual(requireConfig.someConfigSwitch, 0);
					assert.strictEqual(requireConfig.isDebug, 1);
					assert.strictEqual(requireConfig.anotherConfigSwitch, 2);

					assert.strictEqual(data.has['config-someConfigSwitch'], 0);
					assert.strictEqual(data.has['config-isDebug'], 1);
					assert.strictEqual(data.has['config-anotherConfigSwitch'], 2);
					assert.strictEqual(data.has['some-has-feature'], 5);
				}).executeAsync(function (callback) {
					// setting an existing config variable after boot does *not* affect the has cache
					require([ 'dojo/has' ], function (has) {
						require({ someConfigSwitch: 3 });
						callback({
							requireConfig: require.rawConfig,
							'config-someConfigSwitch': has('config-someConfigSwitch')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data.requireConfig.someConfigSwitch, 3);
					assert.strictEqual(data['config-someConfigSwitch'], 0);
				}).executeAsync(function (callback) {
					// but, we can add new configfeatures any time
					require([ 'dojo/has' ], function (has) {
						require({ someNewConfigSwitch: 4 });
						callback({
							requireConfig: require.rawConfig,
							'config-someNewConfigSwitch': has('config-someNewConfigSwitch')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data.requireConfig.someNewConfigSwitch, 4);
					assert.strictEqual(data['config-someNewConfigSwitch'], 4);
				}).executeAsync(function (callback) {
					// setting an existing has feature via config after boot does *not* affect the has cache
					require([ 'dojo/has' ], function (has) {
						require({ has: { 'some-has-feature': 6 } });
						callback({
							'some-has-feature': has('some-has-feature')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data['some-has-feature'], 5);
				}).executeAsync(function (callback) {
					// setting an existing has feature via has.add does *not* affect the has cache...
					require([ 'dojo/has' ], function (has) {
						has.add('some-has-feature', 6);
						callback({
							'some-has-feature': has('some-has-feature')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data['some-has-feature'], 5);
				}).executeAsync(function (callback) {
					// ...*unless* you use force...
					require([ 'dojo/has' ], function (has) {
						has.add('some-has-feature', 6, 0, 1);
						callback({
							'some-has-feature': has('some-has-feature')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data['some-has-feature'], 6);
				}).executeAsync(function (callback) {
					// but, we can add new has features any time
					require([ 'dojo/has' ], function (has) {
						require({ has: { 'some-new-has-feature': 7 } });
						callback({
							'some-new-has-feature': has('some-new-has-feature')
						});
					});
				}).then(function (data) {
					assert.strictEqual(data['some-new-has-feature'], 7);
				});
			}
		),

		'declare steps on provide': loaderTest(
			require.toUrl('./loader/index.html'),
			{
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' },
					{ name: 'testing', location: '.' }
				],
				map: {
					'*': {
						'dojo/tests': 'testing/tests/functional'
					}
				}
			},
			function (callback) {
				require([
					'dojo',
					'dojo/tests/_base/loader/declareStepsOnProvideAmd'
				], function (dojo, DeclareStepsOnProvideAmd) {
					var data = {};
					var instance = new DeclareStepsOnProvideAmd();
					data.status1 = instance.status();

					// requiring declareStepsOnProvideAmd caused
					// declareStepsOnProvide to load which loaded *two* modules
					// and dojo.declare stepped on both of them
					instance = new (require('dojo/tests/_base/loader/declareStepsOnProvide1'))();
					data.status2 = instance.status();
					callback(data);
				});
			},
			function (data) {
				assert.strictEqual(data.status1, 'OK');
				assert.strictEqual(data.status2, 'OK-1');
			}
		),

		'publish require result': (function () {
			function createPublishTest(publish) {
				return loaderTest(
					require.toUrl('./loader/index.html'),
					{
						publishRequireResult: Boolean(publish),
						packages: [
							{ name: 'dojo', location: 'node_modules/dojo' },
							{ name: 'testing', location: '.' }
						],
						map: {
							'*': {
								'dojo/tests': 'testing/tests/functional'
							}
						}
					},
					function (callback) {
						var dojo = window.dojo;

						dojo.setObject('dojo.tests._base.loader.pub1', 'do-not-mess-with-me');
						dojo.require('dojo.tests._base.loader.pub1');
						dojo.require('dojo.tests._base.loader.pub2');

						require([ 'dojo/has' ], function (has) {
							callback({
								pub1: dojo.getObject('dojo.tests._base.loader.pub1'),
								pub2Status: dojo.getObject('dojo.tests._base.loader.pub2') &&
									dojo.getObject('dojo.tests._base.loader.pub2.status'),
								dojoConfigPublishRequireResult: !!dojo.config.publishRequireResult,
								hasPublishRequireResult: !!has('config-publishRequireResult')
							});
						});
					},
					function (data) {
						assert.strictEqual(data.pub1, 'do-not-mess-with-me');

						if (publish) {
							assert.isTrue(data.hasPublishRequireResult);
							assert.strictEqual(data.pub2Status, 'ok');
						}
						else {
							assert.isFalse(data.hasPublishRequireResult);
							assert.isFalse(data.dojoConfigPublishRequireResult);
							assert.isTrue(data.pub2Status == null);
						}
					}
				);
			}

			return {
				'publish': createPublishTest(true),
				'no publish': createPublishTest()
			};
		})(),

		'top level module by paths': loaderTest(
			require.toUrl('./loader/index.html'),
			{
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				var myModule1Value = {};
				var myModule2Value = {};

				define('myModule1', [], myModule1Value);
				define('myModule2', [], myModule2Value);

				require({
					aliases: [
						// yourModule --> myModule1
						[ 'yourModule', 'myModule1' ],

						// yourOtherModule --> myModule1
						[ /yourOtherModule/, 'myModule1' ],

						// yourModule/*/special --> yourModule/common/special
						// this will result in a resubmission to finally resolve in the next one
						[ /yourOtherModule\/([^\/]+)\/special/, 'yourOtherModule/common/special' ],

						// yourModule/common/special --> myModule2
						// notice the regex above also finds yourOtherModule/common/special;
						// the extra parenthesized subexprs make this have priority
						[ /(yourOtherModule\/(common))\/special/, 'myModule2' ]
					],
					paths: { myTopLevelModule: './tests/functional/_base/loader/myTopLevelModule' }
				});

				require([
					'myTopLevelModule',
					'myModule1',
					'myModule2',
					'yourModule',
					'yourOtherModule',
					'yourOtherModule/stuff/special'
				], function (myModule, myModule1, myModule2, yourModule, yourOtherModule, special) {
					// top level module via path
					var myTopLevelModule = this.myTopLevelModule;
					var results =
						myModule1Value === myModule1 &&
						myModule1Value === yourModule &&
						myModule1Value === yourOtherModule &&
						myModule2Value === myModule2 &&
						myModule2Value === special &&
						myTopLevelModule.name === 'myTopLevelModule' &&
						myTopLevelModule.myModule.name === 'myTopLevelModule.myModule';
					callback(results);
				});
			},
			function (data) {
				assert.isTrue(data);
			}
		),

		'config/test': loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: true,
				baseUrl: '../../../..',
				packages: [
					{
						name: 'loader',
						location: 'tests/functional/_base/loader/config',
						packageMap: {
							'pkg': 'pkgMapped'
						}
					},
					{
						name: 'pkg',
						location: 'tests/functional/_base/loader/config/pkg'
					},
					{
						name: 'pkgMapped',
						location: 'tests/functional/_base/loader/config/pkg',
						packageMap: {
							'pkg': 'pkgMapped'
						}
					},
					{
						name: 'dojo',
						location: 'node_modules/dojo'
					}
				],
				config: {
					'loader/someModuleConfiggedPriorToBoot': {
						someConfig: 'this is the config for someModuleConfiggedPriorToBoot'
					}
				}
			},
			function (callback) {
				function mixin(destination, source) {
					for (var key in source) {
						if (Object.prototype.hasOwnProperty.call(source, key)) {
							destination[key] = source[key];
						}
					}
					return destination;
				}
				require({
					config: {
						'loader/someModule': {
							someConfig: 'this is the config for someModule-someConfig'
						},
						'pkgMapped/m1': {
							globalConfig: 'globalConfigForpkgMapped/m1',
							isMapped: true
						},
						'pkgMapped/m2': {
							globalConfig: 'globalConfigForpkgMapped/m2'
						}
					}
				});

				require([
					'loader/someModuleConfiggedPriorToBoot',
					'loader/someModule'
				], function (someModuleConfiggedPriorToBoot, someModule) {
					var results = {
						someModuleConfiggedPriorToBootConfig: mixin({}, someModuleConfiggedPriorToBoot.getConfig()),
						someModuleConfig: mixin({}, someModule.getConfig()),
						someModuleM1Config: mixin({}, someModule.m1.getConfig()),
						someModuleM2Config: mixin({}, someModule.m2.getConfig())
					};

					require({
						config: {
							'loader/someModule': {
								someMoreConfig: 'this is the config for someModule-someMoreConfig'
							}
						}
					});

					require(['loader/someModule'], function (someModuleAfterConfig) {
						mixin(results, {
							someModuleAfterConfig: mixin({}, someModuleAfterConfig.getConfig())
						});
						require({
							config: {
								'pkg/m1': { globalConfig: 'globalConfigForM1' },
								'pkg/m2': { globalConfig: 'globalConfigForM2' }
							}
						}, [ 'pkg/m1', 'pkg/m2' ], function (m1, m2) {
							callback(mixin(results, {
								m1Config: m1.getConfig(),
								m2Config: m2.getConfig()
							}));
						});
					});
				});
			},
			function (data) {
				assert.deepEqual(data.someModuleConfiggedPriorToBootConfig, {
					someConfig: 'this is the config for someModuleConfiggedPriorToBoot'
				});
				assert.deepEqual(data.someModuleConfig, {
					someConfig: 'this is the config for someModule-someConfig'
				});
				assert.deepEqual(data.someModuleM1Config, {
					globalConfig: 'globalConfigForpkgMapped/m1',
					isMapped: true,
					configThroughMappedRefForM1: 'configThroughMappedRefForM1'
				});
				assert.deepEqual(data.someModuleM2Config, {
					globalConfig: 'globalConfigForpkgMapped/m2',
					configThroughMappedRefForM1: 'configThroughMappedRefForM1',
					config1: 'mapped-config1',
					config2: 'mapped-config2',
					config3: 'mapped-config3'
				});
				assert.deepEqual(data.someModuleAfterConfig, {
					someConfig: 'this is the config for someModule-someConfig',
					someMoreConfig: 'this is the config for someModule-someMoreConfig'
				});
				assert.deepEqual(data.m1Config, {
					globalConfig: 'globalConfigForM1'
				});
				assert.deepEqual(data.m2Config, {
					globalConfig: 'globalConfigForM2',
					config1: 'config1',
					config2: 'config2',
					config3: 'config3'
				});
			}
		),
		mappingMultiLayer: loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: true,
				baseUrl: '.',
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' },
					{
						name: 'test',
						location: './mapping-multi-layer'
					},
					{
						name: 'app1',
						location: './mapping-multi-layer/App1'
					},
					{
						name: 'app2',
						location: './mapping-multi-layer/App2'
					},
					{
						name: 'common1',
						location: './mapping-multi-layer/Common1'
					},
					{
						name: 'common2',
						location: './mapping-multi-layer/Common2'
					},
					{
						name: 'router',
						location: './mapping-multi-layer/Router'
					},
					{
						name: 'mappedModule',
						location: './mapping-multi-layer/MappedModule'
					}
				],
				map: {
					'app1': {
						'common': 'common1'
					},
					'app2': {
						'common': 'common2'
					},
					'my/replacement/A': {
						'my/A': 'my/A'
					},
					'*': {
						'starmap/demo1': 'router/demoA',
						'starmap/demo2': 'router/demoB',
						'starmapModule': 'mappedModule',
						'my/A': 'my/replacement/A'
					}
				}
			},
			function (callback) {
				// consume pending cache, the following are added at the end of a built dojo.js in a closure
				require({ cache: {} });
				!require.async && require([ 'dojo' ]);
				require.boot && require.apply(null, require.boot);

				// begin test:
				// moving modules from the pending cache to the module cache should ignore
				// any mapping, pathing, or alias rules
				var handle = require.on('error', function () {
					handle.remove();
					callback({ error: true });
				});
				require([ 'test/main' ], function () {
					handle.remove();
					callback({ error: false, results: results });
				});
			},
			function (data) {
				if (data.error) {
					assert.fail("require error");
				}
				else {
					var expected = ["Common1/another:cache", "Router/demoB:nocache", "App1/thing:cache", "Router/demoC:cache", "Router/demoA:cache", "MappedModule/mappedC:cache", "mappedModule/mappedA:cache", "my/B:cache", "my/A:cache", "my/replacement/A:cache", "mainRequire1:loaded", "Common2/anotherone:cache", "Common2/another:cache", "mappedModule/mappedB:cache", "App2/thing:cache", "mainRequire2:loaded"];
					assert.strictEqual(data.results.join(), expected.join());
				}
			}
		),

		mapping: loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: true,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				],
				map: {
					'my/replacement/A': {
						'my/A': 'my/A'
					},
					'*': {
						'my/A': 'my/replacement/A'
					}
				}
			},
			function (callback) {
				// simulate a built layer, this is added to dojo.js by the builder
				require({
					cache: {
						'my/replacement/A': function () {
							define([ '../A' ], function () {
								return { it: 'is a replacement module' };
							});
						},
						'my/A': function () {
							define([ './B' ], function () {
								return { it: 'is the original module' };
							});
						},
						'my/B': function () {
							define([], function () {
								return { it: 'is a module dependency' };
							});
						}
					}
				});

				// consume pending cache, the following are added at the end of a built dojo.js in a closure
				require({ cache: {} });
				!require.async && require([ 'dojo' ]);
				require.boot && require.apply(null, require.boot);

				// begin test:
				// moving modules from the pending cache to the module cache should ignore
				// any mapping, pathing, or alias rules
				var handle = require.on('error', function () {
					handle.remove();
					callback({ error: true });
				});

				require([ 'my/A' ], function (A) {
					handle.remove();
					callback({ aIt: A.it });
				});
			},
			function (data) {
				if (data.error) {
					assert.fail();
				}
				else {
					assert.strictEqual(data.aIt, 'is a replacement module');
				}
			}
		),

		compactPath: loaderTest(
			require.toUrl('./loader/index.html'),
			{ isDebug: 1, async: 1 },
			function (callback) {
				var compactPath = require.compactPath;
				callback([
					compactPath('../../dojo/../../mytests'),
					compactPath('module'),
					compactPath('a/./b'),
					compactPath('a/../b'),
					compactPath('a/./b/./c/./d'),
					compactPath('a/../b/../c/../d'),
					compactPath('a/b/c/../../d'),
					compactPath('a/b/c/././d'),
					compactPath('./a/b'),
					compactPath('../a/b'),
					compactPath('')
				]);
			},
			function (data) {
				assert.strictEqual('../../../mytests', data.shift());
				assert.strictEqual('module', data.shift());
				assert.strictEqual('a/b', data.shift());
				assert.strictEqual('b', data.shift());
				assert.strictEqual('a/b/c/d', data.shift());
				assert.strictEqual('d', data.shift());
				assert.strictEqual('a/d', data.shift());
				assert.strictEqual('a/b/c/d', data.shift());
				assert.strictEqual('a/b', data.shift());
				assert.strictEqual('../a/b', data.shift());
				assert.strictEqual('', data.shift());
			}
		),

		modules: loaderTest(
			require.toUrl('./loader/index.html'),
			{
				async: 1,
				baseUrl: './foo',
				packages: [
					{ name: 'testing', location: '../../../../../' },
					{ name: 'dojo', location: '../../../../../node_modules/dojo' }
				]
			},
			function (callback) {
				require([
					'dojo',
					'dojo/has',
					'modules/anon',
					'modules/wrapped',
					'testing/tests/functional/_base/loader/modules/full',
					'modules/data',
					'modules/factoryArity',
					'modules/factoryArityExports',
					'testing/tests/functional/_base/loader/modules/idFactoryArity',
					'testing/tests/functional/_base/loader/modules/idFactoryArityExports'
				], function (dojo, has, anon, wrapped) {
					callback([
						has('dojo-amd-factory-scan'),
						anon.theAnswer,
						require('modules/anon').five,
						wrapped.five,
						dojo.require('testing.tests.functional._base.loader.modules.wrapped').five,
						require('modules/wrapped').five,
						require('testing/tests/functional/_base/loader/modules/full').twiceTheAnswer,
						require('modules/data').five,

						require('modules/factoryArity').module.id,
						require('modules/factoryArity').id,
						require('modules/factoryArity').impliedDep,

						require('modules/factoryArityExports').module.id,
						require('modules/factoryArityExports').id,
						require('modules/factoryArityExports').impliedDep,

						require('testing/tests/functional/_base/loader/modules/idFactoryArity').module.id,
						require('testing/tests/functional/_base/loader/modules/idFactoryArity').id,
						require('testing/tests/functional/_base/loader/modules/idFactoryArity').impliedDep,

						require('testing/tests/functional/_base/loader/modules/idFactoryArityExports').module.id,
						require('testing/tests/functional/_base/loader/modules/idFactoryArityExports').id,
						require('testing/tests/functional/_base/loader/modules/idFactoryArityExports').impliedDep
					]);
				});
			},
			function (data) {
				assert.strictEqual(data.shift(), 1);
				assert.strictEqual(data.shift(), 42);
				assert.strictEqual(data.shift(), 5);
				assert.strictEqual(data.shift(), 5);
				assert.strictEqual(data.shift(), 5);
				assert.strictEqual(data.shift(), 5);
				assert.strictEqual(data.shift(), 84);
				assert.strictEqual(data.shift(), 5);

				assert.strictEqual(data.shift(), 'modules/factoryArity');
				assert.strictEqual(data.shift(), 'factoryArity');
				assert.strictEqual(data.shift(), 'impliedDep1');

				assert.strictEqual(data.shift(), 'modules/factoryArityExports');
				assert.strictEqual(data.shift(), 'factoryArityExports');
				assert.strictEqual(data.shift(), 'impliedDep2');

				assert.strictEqual(data.shift(), 'testing/tests/functional/_base/loader/modules/idFactoryArity');
				assert.strictEqual(data.shift(), 'idFactoryArity');
				assert.strictEqual(data.shift(), 'impliedDep3');

				assert.strictEqual(data.shift(), 'testing/tests/functional/_base/loader/modules/idFactoryArityExports');
				assert.strictEqual(data.shift(), 'idFactoryArityExports');
				assert.strictEqual(data.shift(), 'impliedDep4');
			}
		)
	});
});
