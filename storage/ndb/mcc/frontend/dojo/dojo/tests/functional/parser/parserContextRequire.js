define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../../support/ready'
], function(require, registerSuite, assert, ready) {
	registerSuite({
		name : 'dojo/parser - context require',

		test : function() {
			return ready(this.get('remote'), require.toUrl('./parserContextRequire.html')).setExecuteAsyncTimeout(50000).executeAsync(function(done, e) {
				require([
					'testing/parser',
					'testing/tests/functional/parser/support/a/AMDWidget',
					'testing/tests/functional/parser/support/a/AMDWidget2',
					'testing/tests/functional/parser/support/b/AMDWidget',
					'testing/tests/functional/parser/support/b/AMDWidget2',
					'dojo/domReady!'
				], function(parser, AMDWidgetA1, AMDWidgetA2, AMDWidgetB1, AMDWidgetB2) {
					try {
						parser.parse().then(function() {
							/* global cr1, cr2 */
							done({
								cr1CorrectType : cr1.isInstanceOf(AMDWidgetA1),
								cr2CorrectType : cr2.isInstanceOf(AMDWidgetB1),
								cr1ChildCorrectType : cr1.child.isInstanceOf(AMDWidgetA2),
								cr2ChildCorrectType : cr2.child.isInstanceOf(AMDWidgetB2)
							});
						}).otherwise(done);
					} catch (e) {
						done({
							e : "x"
						});
					}
				});
			}).then(function(results) {
				assert.deepEqual(results, {
					cr1CorrectType : true,
					cr2CorrectType : true,
					cr1ChildCorrectType : true,
					cr2ChildCorrectType : true
				});
			});
		}
	});
});
