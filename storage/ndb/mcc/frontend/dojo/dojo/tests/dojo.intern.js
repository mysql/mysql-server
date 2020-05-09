// Supplies useLoader with a dojoConfig enabling require.undef()
// dojoConfig needs to be defined here, otherwise it's too late to affect the dojo loader api
/* globals dojoConfig */
/* jshint -W020 */
dojoConfig = {
	async: true,
	tlmSiblingOfDojo: false,
	useDeferredInstrumentation: false,
	has: {
		'dojo-undef-api': true
	}
};

define([
	'intern',
	'intern/dojo/topic',
	'intern/dojo/has!host-node?./services/main'
], function (intern, topic, services) {
	if (services && intern.mode === 'runner') {
		services.start().then(function (server) {
			topic.subscribe('/runner/end', function () {
				server.close();
			});
		});
	}
	return {
		proxyPort: 9000,
		proxyUrl: 'http://localhost:9001/',
		capabilities: {
			'selenium-version': '2.43.0',
			'record-screenshots': false,
			'sauce-advisor': false,
			'video-upload-on-pass': false,
			'max-duration': 300
		},
		environments: [
			{ browserName: 'internet explorer', version: [ '8', '9', '10' ], platform: 'Windows 7',
				'prerun': 'http://localhost:9001/tests/support/prerun.bat' },
			{ browserName: 'internet explorer', version: '10', platform: 'Windows 8',
				'prerun': 'http://localhost:9001/tests/support/prerun.bat' },
			{ browserName: 'internet explorer', version: '11', platform: 'Windows 10',
				'prerun': 'http://localhost:9001/tests/support/prerun.bat' },
			{ browserName: 'microsoftedge', version: '20.10240', platform: 'Windows 10' },
			{ browserName: 'firefox', version: '', platform: [ 'OS X 10.10', 'Windows 7' ] },
			{ browserName: 'chrome', version: '', platform: [
				'OS X 10.10', 'Windows 7'
			] },
			{ browserName: 'safari', version: '6', platform: 'OS X 10.8' },
			{ browserName: 'safari', version: '7', platform: 'OS X 10.9' },
			{ browserName: 'safari', version: '8', platform: 'OS X 10.10' }
		],
		maxConcurrency: 3,
		tunnel: 'SauceLabsTunnel',
		useLoader: {
			'host-node': '../../../../dojo',  // relative path from the launcher
			'host-browser': '../dojo.js'
		},
		loader: {
			packages: [
				// The dojo-under-test
				{ name: 'testing', location: '.' },
				// The dojo used for writing tests
				{ name: 'dojo', location: 'node_modules/dojo' },
				{ name: 'sinon', location: 'node_modules/sinon/pkg', main: 'sinon'}
			],
			map: {
				intern: {
					dojo: 'intern/node_modules/dojo',
					chai: 'intern/node_modules/chai/chai',
					diff: 'intern/node_modules/diff/diff'
				},

				// Tests should use dojo in node_modules
				'testing/tests': {
					dojo: 'dojo',
					// Once this section matches, the star section will not, so intern/dojo needs to be
					// defined here as well
					'intern/dojo': 'intern/node_modules/dojo'
				},
				// Any dojo modules loaded by dojo-under-test modules should come from the dojo-under-test, not the dojo
				// used for writing tests
				testing: {
					dojo: 'testing'
				},
				'*': {
					'intern/dojo': 'intern/node_modules/dojo'
				}
			}
		},
		excludeInstrumentation: intern.args.fast ?
			/./ :
			/(?:^|[\/\\])(?:(?:cldr\/nls|html-report|nls|node_modules|tests|testsDOH)[\/\\]|(?:Gruntfile|package)\.js$)/,
		suites: [ 'testing/tests/unit/all' ],
		functionalSuites: [ 'testing/tests/functional/all' ]
	};
});
