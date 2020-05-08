/* globals domGeometry, aspect, query, domStyle */
define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (require, registerSuite, assert, pollUntil) {
	var FX_URL = './support/fx-nodelist.html';

	function getPage(context, url) {
		return context.get('remote')
			.setExecuteAsyncTimeout(5000)
			.get(require.toUrl(url))
			.then(pollUntil('return ready;'));
	}

	registerSuite({
		name: 'dojo/NodeList-fx',

		'.fadeOut': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var anim = query('p').fadeOut();

					aspect.after(anim, 'onEnd', function () {
						done(query('p').every(function (item) {
							return Number(domStyle.get(item, 'opacity')) === 0;
						}));
					}, true);

					query('p').style('opacity', 1);
					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'.fadeIn': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					var anim = qP.fadeIn();

					aspect.after(anim, 'onEnd', function () {
						done(qP.every(function (item) {
							return Number(domStyle.get(item, 'opacity')) === 1;
						}));
					}, true);

					qP.style('opacity', 0);
					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'.wipeOut': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					var anim = qP.wipeOut();

					aspect.after(anim, 'onEnd', function () {
						done(qP.every(function (item) {
							return domGeometry.position(item).h === 0;
						}));
					}, true);

					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'.wipeIn': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					var anim = qP.wipeIn();

					aspect.after(anim, 'onEnd', function () {
						done(qP.every(function (item) {
							// FIXME: need a more robust test for "have wiped all the way in"
							return domGeometry.position(item).h > 0;
						}));
					}, true);

					qP.style('height', 0);
					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'.slideTo': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					var anim = qP.slideTo({ left: 500 });

					aspect.after(anim, 'onEnd', function () {
						done(qP.every(function (item) {
							return domGeometry.getMarginBox(item).l === 500;
						}));
					}, true);

					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'.anim': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					var anim = qP.anim({ width: 500 });

					aspect.after(anim, 'onEnd', function () {
						done(qP.every(function (item) {
							return domGeometry.getMarginBox(item).w === 500;
						}));
					}, true);

					qP.style('position', '');
					qP.style('left', '');
					anim.play();
				})
				.then(function (results) {
					assert.isTrue(results);
				});
		},

		'auto setting is true': function () {
			return getPage(this, FX_URL)
				.executeAsync(function (done) {
					var qP = query('p');
					qP.fadeOut({
						auto: true,
						onEnd: done
					})
					// past here we're expecting a NodeList back, not an Animation
					.at(0);
				});
		}
	});
});
