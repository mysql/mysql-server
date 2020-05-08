define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'./support/loaderTest'
], function (require, registerSuite, assert, loaderTest) {
	var built = 1;
	//>>excludeStart("srcVersion", kwArgs.copyTests=="build");

	built = 0;
	//>>excludeEnd("srcVersion");
	function createXDomainTest(async, variation) {
		variation = variation || 2;
		var expectedSequence;
		if ((async === 'legacyAsync' || built) && variation === 2) {
			expectedSequence = [
				'local1-5',
				'local1-17',
				'local2-1',
				'local2-2',
				'local2-3',
				'local3-1',
				'local3-2',
				'local1-1',
				'local1-2',
				'local1-3',
				'local1-dep-1',
				'local1-dep-2',
				'local1-4',
				'local1-6',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-1',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-2',
				'local1-7',
				'local1-8',
				'dojo.tests._base.loader.xdomain.local1-browser-1',
				'dojo.tests._base.loader.xdomain.local1-browser-2',
				'dojo.tests._base.loader.xdomain.local1-browser-skip-1',
				'local1-9',
				'local1-10',
				'local1-11',
				'local1-12',
				'local1-13',
				'local1-14',
				'local1-15',
				'local1-16',
				'local1-18',
				'local3-3',
				'local2-4'
			];
		}
		else if (async === 0 && variation === 2) {
			expectedSequence = [
				'local2-1',
				'local2-2',
				'local2-3',
				'local2-4',
				'local1-5',
				'local1-17',
				'local3-1',
				'local3-2',
				'local1-1',
				'local1-2',
				'local1-3',
				'local1-dep-1',
				'local1-dep-2',
				'local1-4',
				'local1-6',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-1',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-2',
				'local1-7',
				'local1-8',
				'dojo.tests._base.loader.xdomain.local1-browser-1',
				'dojo.tests._base.loader.xdomain.local1-browser-2',
				'dojo.tests._base.loader.xdomain.local1-browser-skip-1',
				'local1-9',
				'local1-10',
				'local1-11',
				'local1-12',
				'local1-13',
				'local1-14',
				'local1-15',
				'local1-16',
				'local1-18',
				'local3-3'
			];
		}
		else if ((async === 'legacyAsync' || built) && variation === 1) {
			expectedSequence = [
				'local1-5',
				'local1-17',
				'local1-1',
				'local1-2',
				'local1-3',
				'local1-dep-1',
				'local1-dep-2',
				'local1-4',
				'local1-6',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-1',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-2',
				'local1-7',
				'local1-8',
				'dojo.tests._base.loader.xdomain.local1-browser-1',
				'dojo.tests._base.loader.xdomain.local1-browser-2',
				'dojo.tests._base.loader.xdomain.local1-browser-skip-1',
				'local1-9',
				'local1-10',
				'local1-11',
				'local1-12',
				'local1-13',
				'local1-14',
				'local1-15',
				'local1-16',
				'local1-18'
			];
		}
		else if (async === 0 && variation === 1) {
			expectedSequence = [
				'local1-1',
				'local1-2',
				'local1-3',
				'local1-4',
				'local1-5',
				'local1-6',
				'local1-7',
				'local1-8',
				'local1-9',
				'local1-10',
				'local1-11',
				'local1-12',
				'local1-13',
				'local1-14',
				'local1-15',
				'local1-16',
				'local1-17',
				'local1-18',
				'local1-dep-1',
				'local1-dep-2',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-1',
				'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-2',
				'dojo.tests._base.loader.xdomain.local1-browser-1',
				'dojo.tests._base.loader.xdomain.local1-browser-2',
				'dojo.tests._base.loader.xdomain.local1-browser-skip-1'
			];
		}
		return loaderTest(
			require.toUrl('./index.html'),
			{
				async: async || 0,
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
			[
				function (async, variation, callback) {
					function makeResults(actual, expected, message) {
						return {
							actual: actual,
							expected: expected,
							message: message
						};
					}

					var xdomainExecSequence = window.xdomainExecSequence = [];
					var xdomainLog = window.xdomainLog = [];

					define('dijit', [ 'dojo' ], function (dojo) {
						return dojo.dijit;
					});
					define('dojox', [ 'dojo' ], function (dojo) {
						return dojo.dojox;
					});

					require([ 'dojo', 'dojo/ready' ], function (dojo, ready) {
						if (!('legacyMode' in require)) {
							callback(JSON.stringify({
								tests: [makeResults(define.vendor, 'dojotoolkit.org')],
								sequence: xdomainExecSequence
							}));
							return;
						}

						require.isXdUrl = function (url) {
							return !/loader\/xdomain/.test(url) && !/syncBundle/.test(url);
						};

						if (variation === 1) {
							dojo.require('dojo.tests._base.loader.xdomain.local1');

							if (async === 0) {
								xdomainLog.push(11, dojo.tests._base.loader.xdomain.local1 === 'stepOnLocal1');
								xdomainLog.push(12, dojo.require('dojo.tests._base.loader.xdomain.local1') === 'stepOnLocal1');
								xdomainLog.push(13, require('dojo/tests/_base/loader/xdomain/local1') === 'stepOnLocal1');
							}
							else {
								xdomainLog.push(11, dojo.getObject('dojo.tests._base.loader.xdomain.local1') === undefined);
							}
						}
						else {
							dojo.require('dojo.tests._base.loader.xdomain.local2');
							if (async === 0) {
								xdomainLog.push(11, dojo.tests._base.loader.xdomain.local2.status === 'local2-loaded');
								xdomainLog.push(12, dojo.require('dojo.tests._base.loader.xdomain.local2').status === 'local2-loaded');
								xdomainLog.push(13, require('dojo/tests/_base/loader/xdomain/local2').status === 'local2-loaded');
							}
							else {
								xdomainLog.push(11, dojo.getObject('dojo.tests._base.loader.xdomain.local2') === undefined);
							}

							xdomainLog.push(16, dojo.getObject('dojo.tests._base.loader.xdomain.local1') === undefined);

							if (dojo.isIE !== 6) {
								try {
									require('dojo/tests/_base/loader/xdomain/local1');
									xdomainLog.push(19, false);
								}
								catch (e) {
									xdomainLog.push(19, true);
								}
							}
						}

						xdomainLog.push(14, (dojo.hash === undefined));
						xdomainLog.push(15, (dojo.cookie === undefined));
						xdomainLog.push(17, dojo.getObject('dojo.tests._base.loader.xdomain.local3') === undefined);

						if (dojo.isIE !== 6) {
							try {
								require('dojo/tests/_base/loader/xdomain/local3');
								xdomainLog.push(18, false);
							}
							catch (e) {
								xdomainLog.push(18, true);
							}
						}

						ready(function () {
							var results = [], throwTest = false;
							for (var i = 0; i < xdomainLog.length; i += 2) {
								results.push(makeResults(!!xdomainLog[i + 1], true, 'failed at id = ' + xdomainLog[i]));
							}

							results.push(makeResults(dojo.tests._base.loader.xdomain.local1, 'stepOnLocal1'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1'), 'stepOnLocal1'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1'), 'stepOnLocal1'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1'), 'stepOnLocal1'));

							results.push(makeResults(dojo.tests._base.loader.xdomain.local1SteppedOn, 'stepOn1SteppedOn'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1SteppedOn'), 'stepOn1SteppedOn'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1SteppedOn'), 'stepOn1SteppedOn'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1SteppedOn'), 'stepOn1SteppedOn'));

							results.push(makeResults(dojo.tests._base.loader.xdomain.local1NotSteppedOn.status, 'local1NotSteppedOn'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1NotSteppedOn.status'), 'local1NotSteppedOn'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1NotSteppedOn').status, 'local1NotSteppedOn'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1NotSteppedOn').status, 'local1NotSteppedOn'));

							results.push(makeResults(dojo.tests._base.loader.xdomain['local1-dep'].status, 'dojo.tests._base.loader.xdomain.local1-dep-ok'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1-dep.status'), 'dojo.tests._base.loader.xdomain.local1-dep-ok'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1-dep').status, 'dojo.tests._base.loader.xdomain.local1-dep-ok'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1-dep').status, 'dojo.tests._base.loader.xdomain.local1-dep-ok'));

							results.push(makeResults(dojo.tests._base.loader.xdomain['local1-runtimeDependent1'].status, 'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-ok'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1-runtimeDependent1.status'), 'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-ok'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1-runtimeDependent1').status, 'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-ok'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1-runtimeDependent1').status, 'dojo.tests._base.loader.xdomain.local1-runtimeDependent1-ok'));

							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1-runtimeDependent').status, 'ok'));

							results.push(makeResults(dojo.tests._base.loader.xdomain['local1-runtimeDependent2'], undefined));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1-runtimeDependent2'), undefined));

							try {
								require('dojo/tests/_base/loader/xdomain/local1/runtimeDependent2');
							} catch (e) {
								throwTest = true;
							}
							results.push(makeResults(throwTest, true));

							results.push(makeResults(dojo.tests._base.loader.xdomain['local1-browser'].status, 'dojo.tests._base.loader.xdomain.local1-browser-ok'));
							results.push(makeResults(dojo.getObject('dojo.tests._base.loader.xdomain.local1-browser.status'), 'dojo.tests._base.loader.xdomain.local1-browser-ok'));
							results.push(makeResults(dojo.require('dojo.tests._base.loader.xdomain.local1-browser').status, 'dojo.tests._base.loader.xdomain.local1-browser-ok'));
							results.push(makeResults(require('dojo/tests/_base/loader/xdomain/local1-browser').status, 'dojo.tests._base.loader.xdomain.local1-browser-ok'));

							results.push(makeResults(!!dojo.cookie, true));
							results.push(makeResults(dojo.getObject('dojo.cookie'), dojo.cookie));
							results.push(makeResults(require('dojo/cookie'), dojo.cookie));

							if (variation !== 1) {
								results.push(makeResults(!!dojo.hash, true));
								results.push(makeResults(dojo.getObject('dojo.hash'), dojo.hash));
								results.push(makeResults(require('dojo/hash'), dojo.hash));
							}

							callback(JSON.stringify({
								tests: results,
								sequence: xdomainExecSequence
							}));
						});
					});
				},
				[ async, variation ]
			],
			function (data) {
				data = JSON.parse(data);
				var test;
				while ((test = data.tests.shift())) {
					if (typeof test.actual === 'object') {
						assert.deepEqual(test.actual, test.expected, test.message);
					}
					else {
						assert.strictEqual(test.actual, test.expected, test.message);
					}
				}
				assert.deepEqual(data.sequence, expectedSequence);
			}
		);
	}

	registerSuite({
		name: 'dojo/_base/loader - xdomain',

		'sync 1': createXDomainTest(0, 1),
		'sync 2': createXDomainTest(0, 2),
		'async 1': createXDomainTest('legacyAsync', 1),
		'async 2': createXDomainTest('legacyAsync', 2)
	});
});
