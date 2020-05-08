define([
	'intern!object',
	'intern/chai!assert',
	'require'
], function (registerSuite, assert, require) {
	var dfd;
	registerSuite({
		name: 'dojo/node',

		'!util': function () {
			dfd = this.async();
			require(['../../node!util'], dfd.callback(function (util) {
				assert('puts' in util, 'this is the built in node module');
			}));
		},

		'!missing': function () {
			assert.throws(function () {
				require(['../../node!missing']);
			}, /Cannot find module 'missing'/);
		},

		'nodemodule': function () {
			dfd = this.async();
			require(['../../node!./resources/nodemodule'], dfd.callback(function (nodemodule) {
				assert('test' in nodemodule, 'module loaded');
				assert.strictEqual(nodemodule.test, 'value', 'object has expected value');
			}));
		},

		'noderequire': function () {
			dfd = this.async();
			require(['../../node!./resources/noderequire'], dfd.callback(function (noderequire) {
				assert('test' in noderequire, 'module loaded');
				assert.strictEqual(noderequire.test, 'value', 'object has expected value');
			}));
		},

		'nodemod': function () {
			dfd = this.async();
			require(['../../node!./resources/nodemod'], dfd.callback(function (nodemod) {
				assert('test' in nodemod, 'module loaded');
				assert.strictEqual(nodemod.test, 'value', 'object has expected value');
			}));
		},

		'noderequireamd': function () {
			dfd = this.async();
			require(['../../node!./resources/noderequireamd'], dfd.callback(function (noderequireamd) {
				assert.strictEqual(noderequireamd.nodeamd.test, 'foo', 'module loaded');
			}));
		}
	});
});
