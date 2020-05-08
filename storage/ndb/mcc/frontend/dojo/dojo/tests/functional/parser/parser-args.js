define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../../support/ready'
], function (require, registerSuite, assert, ready) {
	registerSuite({
		name: 'dojo/parser - arguments',

		scope: function () {
			var remote = this.get('remote');
			return ready(remote, require.toUrl('./parser-args.html'))
				.execute(function () {
					var widgets = window.scopedWidgets;
					return {
						widgetsLength: widgets.length,
						strProp1: widgets[0].strProp1 + ", " + widgets[1].strProp1
					};
				}).then(function (results) {
					assert.deepEqual(results, {
						widgetsLength: 2,
						strProp1: 'text1, text2'
					});
				});
		}
	});
});
