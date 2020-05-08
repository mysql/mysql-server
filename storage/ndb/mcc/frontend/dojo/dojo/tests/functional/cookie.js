define([
	'require',
	'intern!object',
	'intern/chai!assert'
], function (require, registerSuite, assert) {
	registerSuite({
		name: 'dojo/cookie',

		setup: function () {
			return this.get('remote')
				.setExecuteAsyncTimeout(10000)
				.get(require.toUrl('./support/standard.html'));
		},

		beforeEach: function () {
			return this.get('remote').clearCookies();
		},

		set: {
			'one new cookie': function () {
				return this.get('remote').executeAsync(function (done) {
					require(['dojo/cookie'], function (cookie) {
						var cookieName = 'dojo_test';
						var cookieValue = 'test value';

						cookie(cookieName, cookieValue);
						done(document.cookie);
					});
				}).then(function (cookieStr) {
					var cookieName = 'dojo_test';
					var cookieValue = encodeURIComponent('test value');
					var regExp = new RegExp(cookieName + '=([^;]*)');
					var results;

					assert.isDefined(cookieStr);
					assert.isTrue(cookieStr.indexOf(cookieName + '=') >= 0);
					results = cookieStr.match(regExp);
					assert.lengthOf(results, 2);
					assert.equal(results[1], cookieValue);
				});
			},

			'a cookie with a negative expires': function () {
				return this.get('remote').executeAsync(function (done) {
					require(['dojo/cookie'], function (cookie) {
						// set a cookie with a numerical expires
						cookie('dojo_num', 'foo', { expires: 10 });
						done(cookie('dojo_num'));
					});
				}).then(function (actual) {
					assert.isNotNull(actual);
				}).executeAsync(function (done) {
					require(['dojo/cookie'], function (cookie) {
						// remove the cookie by setting it with a negative
						// numerical expires. value doesn't really matter here
						cookie('dojo_num', '-deleted-', { expires: -10 });
						done(cookie('dojo_num'));
					});
				}).then(function (actual) {
					assert.isNull(actual);
				});
			}
		},

		get: {
			'an existing cookie': function () {
				return this.get('remote').executeAsync(function (done) {
					require(['dojo/cookie'], function (cookie) {
						// set the cookie
						var cookieName = 'dojo_test';
						var cookieValue = 'an existing cookie';
						document.cookie = cookieName + '=' + cookieValue;

						done(cookie(cookieName));
					});
				}).then(function (cookieValue) {
					assert.equal(cookieValue, 'an existing cookie');
				});
			}
		},

		'add and remove two new cookies with the same suffix': function () {
			return this.get('remote').executeAsync(function (done) {
				require(['dojo/cookie'], function (cookie) {
					// set two cookies with the same suffix
					cookie('user', '123', { expires: 10 });
					cookie('xuser', 'abc', { expires: 10 });

					done({
						cookie: {
							user: cookie('user'),
							xuser: cookie('xuser')
						}
					});
				});
			}).then(function (actual) {
				assert.equal(actual.cookie.user, '123');
				assert.equal(actual.cookie.xuser, 'abc');
			}).executeAsync(function (done) {
				require(['dojo/cookie'], function (cookie) {
					// remove the cookie by setting it with a negative
					// numerical expires. value doesn't really matter here
					cookie('user', '-deleted-', { expires: -10 });
					cookie('xuser', '-deleted-', { expires: -10 });
					done({
						cookie: {
							user: cookie('user'),
							xuser: cookie('xuser')
						}
					});
				});
			}).then(function (actual) {
				assert.isNull(actual.cookie.user);
				assert.isNull(actual.cookie.xuser);
			});
		}
	});
});
