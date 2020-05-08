/* globals Toggler, fx, aspect */
define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (require, registerSuite, assert, pollUntil) {
	var FX_URL = '../support/fx.html';
	
	function getPage(context, url) {
		return context.get('remote')
			.setExecuteAsyncTimeout(5000)
			.get(require.toUrl(url))
			.then(pollUntil('return ready;'));
	}
	
	registerSuite({
		name: 'dojo/fx/Toggler',

		'.show': {
			'show after hide': function () {
				return getPage(this, FX_URL)
					.executeAsync(function (done) {
						var toggleAnim = new Toggler({
							node: 'foo',
							hideDuration: 100,
							hideFunc: fx.wipeOut,
							showFunc: fx.wipeIn
						});

						toggleAnim.hide();
						setTimeout(function () {
							var showAnim = toggleAnim.show();
							aspect.after(showAnim, 'onEnd', function () {
								done(true);
							});
						});
					})
					.then(function (results) {
						assert.isTrue(results);
					});
			}
		}
	});
});
