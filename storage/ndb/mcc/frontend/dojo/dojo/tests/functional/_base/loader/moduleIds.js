/*jshint -W101*/
define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'./support/loaderTest'
], function (require, registerSuite, assert, loaderTest) {

	function checkResults(data) {
		data = JSON.parse(data);
		var test;
		while ((test = data.shift())) {
			assert.strictEqual(test.actual, test.expected);
		}
	}

	registerSuite({
		name: 'dojo/_base/loader/moduleIds',

		testModuleIds: loaderTest(
			require.toUrl('./index.html'),
			{
				async: 1,
				packages: [
					{
						// canonical...
						name: 'pack1',
						location: '../packages/pack1Root'
					},
					{
						// nonstandard main
						name: 'pack2',
						main: 'pack2Main',
						location: '/pack2Root'
					},
					{
						// nonstandard main
						name: 'pack3',
						main: 'public/main',
						location: '/pack3Root'
					}
				]
			},
			function (callback) {
				function get(mid, refmod) {
					return require.getModuleInfo(mid, refmod, require.packs, require.modules, '../../dojo/',
						require.mapProgs, require.pathsMapProg, 1);
				}

				var data = [];

				function check(result, expectedPid, expectedMidSansPid, expectedUrl) {
					data.push({ actual: result.pid, expected: expectedPid });
					data.push({ actual: result.mid, expected: expectedPid + '/' + expectedMidSansPid });
					data.push({ actual: result.url, expected: expectedUrl + '.js' });
				}

				// non-relative module id resolution...
				var pack1Root = '../../packages/pack1Root/';

				// the various mains...
				check(get('pack1'), 'pack1', 'main', pack1Root + 'main');
				check(get('pack2'), 'pack2', 'pack2Main', '/pack2Root/pack2Main');
				check(get('pack3'), 'pack3', 'public/main', '/pack3Root/public/main');

				// modules...
				check(get('pack1/myModule'), 'pack1', 'myModule', pack1Root + 'myModule');
				check(get('pack2/myModule'), 'pack2', 'myModule', '/pack2Root/myModule');
				check(get('pack3/myModule'), 'pack3', 'myModule', '/pack3Root/myModule');

				// relative module id resolution; relative to module in top-level
				var refmod = {mid: 'pack1/main', pack: require.packs.pack1};
				check(get('.', refmod), 'pack1', 'main', pack1Root + 'main');
				check(get('./myModule', refmod), 'pack1', 'myModule', pack1Root + 'myModule');
				check(get('./myModule/mySubmodule', refmod), 'pack1', 'myModule/mySubmodule', pack1Root + 'myModule/mySubmodule');

				// relative module id resolution; relative to module
				refmod = { mid: 'pack1/sub/publicModule', pack: require.packs.pack1 };
				check(get('.', refmod), 'pack1', 'sub', pack1Root + 'sub');
				check(get('./myModule', refmod), 'pack1', 'sub/myModule', pack1Root + 'sub/myModule');
				check(get('..', refmod), 'pack1', 'main', pack1Root + 'main');
				check(get('../myModule', refmod), 'pack1', 'myModule', pack1Root + 'myModule');
				check(get('../util/myModule', refmod), 'pack1', 'util/myModule', pack1Root + 'util/myModule');

				callback(JSON.stringify(data));
			},
			checkResults
		),

		baseUrl: loaderTest(
			require.toUrl('./index.html'),
			{
				async: 1,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				require(['dojo'], function (dojo) {
					callback({
						originalBaseUrl: dojo.config.baseUrl || './',
						baseUrl: dojo.baseUrl
					});
				});
			},
			function (data) {
				assert.strictEqual(data.originalBaseUrl, data.baseUrl);
			}
		),

		moduleUrl: loaderTest(
			require.toUrl('./index.html'),
			{
				async: 1,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				require(['dojo'], function (dojo) {
					callback({
						expected: require.toUrl('base/tests/myTest.html'),
						moduleUrl1: dojo.moduleUrl(),
						moduleUrl2: dojo.moduleUrl(null),
						moduleUrl3: dojo.moduleUrl(null, 'myTest.html'),
						moduleUrl4: dojo.moduleUrl('base.tests'),
						moduleUrl5: dojo.moduleUrl('base.tests', 'myTest.html')
					});
				});
			},
			function (data) {
				var expected = data.expected;
				assert.isNull(data.moduleUrl1);
				assert.isNull(data.moduleUrl2);
				assert.isNull(data.moduleUrl3);

				// note we expect a trailing slash
				assert.strictEqual(expected.substring(0, expected.length - 11), data.moduleUrl4);
				assert.strictEqual(expected, data.moduleUrl5);
			}
		),

		modulePaths: loaderTest(
			require.toUrl('./index.html'),
			{
				async: 1,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				require(['dojo'], function (dojo) {
					dojo.registerModulePath('mycoolmod', '../some/path/mycoolpath');
					dojo.registerModulePath('mycoolmod.widget',
						'http://some.domain.com/another/path/mycoolpath/widget');

					callback({
						expectedUtilUrl: require.compactPath(require.baseUrl + '../some/path/mycoolpath/util/'),
						moduleUrl1: dojo.moduleUrl('mycoolmod.util'),
						moduleUrl2: dojo.moduleUrl('mycoolmod.widget'),
						moduleUrl3: dojo.moduleUrl('mycoolmod.widget.thingy')
					});
				});
			},
			function (data) {
				assert.strictEqual(data.moduleUrl1, data.expectedUtilUrl);
				assert.strictEqual(data.moduleUrl2, 'http://some.domain.com/another/path/mycoolpath/widget/');
				assert.strictEqual(data.moduleUrl3,
					'http://some.domain.com/another/path/mycoolpath/widget/thingy/');
			}
		),

		moduleUrls: loaderTest(
			require.toUrl('./index.html'),
			{
				async: 1,
				packages: [
					{ name: 'dojo', location: 'node_modules/dojo' }
				]
			},
			function (callback) {
				var data = [];

				function check(actual, expected) {
					data.push({
						actual: actual,
						expected: expected
					});
				}

				require(['dojo', 'dojo/_base/url'], function (dojo) {
					dojo.registerModulePath('mycoolmod', 'some/path/mycoolpath');
					dojo.registerModulePath('mycoolmod2', '/some/path/mycoolpath2');
					dojo.registerModulePath('mycoolmod.widget', 'http://some.domain.com/another/path/mycoolpath/widget');
					dojo.registerModulePath('ipv4.widget', 'http://ipv4user:ipv4passwd@some.domain.com:2357/another/path/ipv4/widget');
					dojo.registerModulePath('ipv6.widget', 'ftp://ipv6user:ipv6passwd@[::2001:0db8:3c4d:0015:0:0:abcd:ef12]:1113/another/path/ipv6/widget');
					dojo.registerModulePath('ipv6.widget2', 'https://[0:0:0:0:0:1]/another/path/ipv6/widget2');

					var basePrefix = require.baseUrl;

					check(dojo.moduleUrl('mycoolmod', 'my/favorite.html'),
						require.compactPath(basePrefix + 'some/path/mycoolpath/my/favorite.html'));
					check(dojo.moduleUrl('mycoolmod', 'my/favorite.html'),
						require.compactPath(basePrefix + 'some/path/mycoolpath/my/favorite.html'));
					check(dojo.moduleUrl('mycoolmod2', 'my/favorite.html'),
						'/some/path/mycoolpath2/my/favorite.html');

					check(dojo.moduleUrl('mycoolmod2.my', 'favorite.html'),
						'/some/path/mycoolpath2/my/favorite.html');

					check(dojo.moduleUrl('mycoolmod.widget', 'my/favorite.html'),
						'http://some.domain.com/another/path/mycoolpath/widget/my/favorite.html');

					check(dojo.moduleUrl('mycoolmod.widget.my', 'favorite.html'),
						'http://some.domain.com/another/path/mycoolpath/widget/my/favorite.html');

					// individual component testing
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).uri,
						'http://ipv4user:ipv4passwd@some.domain.com:2357/another/path/ipv4/widget/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).scheme, 'http');

					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).authority,
						'ipv4user:ipv4passwd@some.domain.com:2357');

					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).user, 'ipv4user');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).password, 'ipv4passwd');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).host, 'some.domain.com');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html'))).port, '2357');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html?query'))).path,
						'/another/path/ipv4/widget/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html?q =somequery'))).query, 'q =somequery');
					check((new dojo._Url(dojo.moduleUrl('ipv4.widget', 'components.html#fragment'))).fragment, 'fragment');

					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).uri,
						'ftp://ipv6user:ipv6passwd@[::2001:0db8:3c4d:0015:0:0:abcd:ef12]:1113/another/path/ipv6/widget/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).scheme, 'ftp');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).authority,
						'ipv6user:ipv6passwd@[::2001:0db8:3c4d:0015:0:0:abcd:ef12]:1113');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).user, 'ipv6user');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).password, 'ipv6passwd');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).host,
						'::2001:0db8:3c4d:0015:0:0:abcd:ef12');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html'))).port, '1113');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html?query'))).path,
						'/another/path/ipv6/widget/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html?somequery'))).query, 'somequery');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget', 'components.html?somequery#somefragment'))).fragment,
						'somefragment');

					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).uri,
						'https://[0:0:0:0:0:1]/another/path/ipv6/widget2/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).scheme, 'https');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).authority, '[0:0:0:0:0:1]');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).user, null);
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).password, null);
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).host, '0:0:0:0:0:1');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).port, null);
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).path,
						'/another/path/ipv6/widget2/components.html');
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).query, null);
					check((new dojo._Url(dojo.moduleUrl('ipv6.widget2', 'components.html'))).fragment, null);

					callback(JSON.stringify(data));
				});
			},
			checkResults
		)
	});
});
