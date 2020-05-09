define([
	'require',
	'intern!object',
	'intern/chai!assert'
], function (require, registerSuite, assert) {
	registerSuite(function () {
		return{
			name: 'dojo/io/script',

			'beforeEach': function () {
				return this.get('remote')
					.setExecuteAsyncTimeout(5000)
					.get(require.toUrl('../support/standard.html'));
			},

			'.get': {
				'basic usage': function () {
					return this.get('remote')
						.executeAsync(function (done) {
							require(['dojo/io/script'], function (script) {
								var varname = 'basic_usage';
								var dfd = script.get({
									url: '/__services/request/script?scriptVar=' + varname
								});

								dfd.then(function () {
									done(window[varname]);
								});
							});
						})
						.then(function (result) {
							assert.equal(result, 'loaded');
						});
				},

				'checkString looks for a variable to be defined': function () {
					return this.get('remote')
						.executeAsync(function (done) {
							require(['dojo/io/script'], function (script) {
								var varname = 'myTasks';
								var dfd = script.get({
									url: '/__services/request/script?scriptVar=' + varname,
									checkString: varname
								});

								dfd.then(function () {
									if (window.hasOwnProperty(varname))
										done(window[varname]);
									else
										done();
								});
							});
						})
						.then(function (result) {
							assert.equal(result, 'loaded');
						});
				},

				'jsonp': function () {
					return this.get('remote')
						.executeAsync(function (done) {
							require(['dojo/io/script'], function (script) {
								var varname = 'jsonp';
								var dfd = script.get({
									url: '/__services/request/script?scriptVar=' + varname,
									content: { foo: 'bar' },
									jsonp: 'callback'
								});

								dfd.then(function (res) {
									done(res);
								});
							});
						})
						.then(function (result) {
							assert.equal(result.animalType, 'mammal');
						});
				},

				'jsonp timeout': function () {
					return this.get('remote')
						.executeAsync(function (done) {
							require(['dojo/io/script'], function (script) {
								script.get({
									url: '/__services/request/script?scriptVar=potato',
									callbackParamName: 'callback',
									content: { delay: 750 },
									timeout: 250,
									handleAs: 'json',
									preventCache: true,
									handle: function (error, result) {
										done(error instanceof Error);
									}
								});
							});
						})
						.then(function (result) {
							assert.isTrue(result);
						});
				}
			}
		};
	});
});