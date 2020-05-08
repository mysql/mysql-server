/* global scriptLoad, myTasks */
define([
	'intern!object',
	'intern/chai!assert',
	'../../../request/script',
	'../../../errors/RequestTimeoutError',
	'../../../errors/CancelError',
	'dojo/domReady!'
], function (registerSuite, assert, script, RequestTimeoutError, CancelError) {
	registerSuite({
		name: 'dojo/request/script',

		load: function () {
			var def = this.async();

			script.get('/__services/request/script', {
				query: {
					scriptVar: 'scriptLoad'
				}
			}).then(
				def.callback(function () {
					assert.notTypeOf(scriptLoad, 'undefined');
					assert.strictEqual(scriptLoad, 'loaded');
				}),
				def.reject
			);
		},

		checkString: function () {
			var def = this.async();

			script.get('/__services/request/script', {
				query: {
					checkString: 'myTasks'
				},
				checkString: 'myTasks'
			}).then(
				def.callback(function () {
					assert.notTypeOf(myTasks, 'undefined');
					assert.deepEqual(
						myTasks,
						[ 'Take out trash.', 'Do dishes.', 'Brush teeth.' ]
					);
				}),
				def.reject
			);
		},

		'script error event': function () {
			var def = this.async();

			script.get('/__services/non-existent-script', {
				jsonp: 'callback',
				timeout: 3000 // timeout for old IE
			}).then(def.reject, def.callback(function (error) {
				var source = error.source;
				if (source.type) {
					assert.strictEqual(source.type, 'error');
				}
				else {
					// old IE doesn't emit an error event, timeout instead
					assert.instanceOf(error, RequestTimeoutError);
				}
			}));
		},

		jsonp: function () {
			var def = this.async();

			script.get('/__services/request/script', {
				query: { foo: 'bar' },
				jsonp: 'callback'
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.animalType, 'mammal');
				}),
				def.reject
			);
		},

		'jsonp timeout': function () {
			var def = this.async();

			script.get('/__services/request/script', {
				query: { delay: 3000 },
				timeout: 500,
				jsonp: 'callback'
			}).then(
				def.reject,
				def.callback(function (error) {
					assert.instanceOf(error, RequestTimeoutError);
				})
			);
		},

		'jsonp cancel': function () {
			var def = this.async();

			script.get('/__services/request/script', {
				query: { delay: 3000 },
				jsonp: 'callback'
			}).then(
				def.reject,
				def.callback(function (error) {
					assert.instanceOf(error, CancelError);
				})
			).cancel();
		}
	});
});
