define([
	'intern!object',
	'intern/chai!assert',
	'dojo/promise/all',
	'../../../request/xhr',
	'../../../request/notify',
	'../../../errors/CancelError'
], function (registerSuite, assert, all, xhr, notify, CancelError) {
	function remover() {
		var args = arguments;

		return function () {
			for (var i=0; i<args.length; i++) {
				args[i].remove();
			}
			return null;
		};
	}

	var handle;

	registerSuite({
		name: 'dojo/request/notify',

		afterEach: function () {
			if (handle) {
				handle = handle();
			}
		},

		'start/send': function () {
			var dfd = this.async(),
				startCount = 0,
				sendCount = 0;

			handle = remover(
				notify('start', function () {
					startCount++;
				}),
				notify('send', function () {
					sendCount++;
				})
			);

			all([
				xhr.get('/__services/request/xhr?delay=1000'),
				xhr.get('/__services/request/xhr?delay=1000')
			]).then(
				dfd.callback(function () {
					assert.strictEqual(startCount, 1);
					assert.strictEqual(sendCount, 2);
				}),
				dfd.reject
			);
		},

		load: function () {
			var dfd = this.async();

			var doneFired = false;
			handle = remover(
				notify('done', function () {
					doneFired = true;
				}),
				notify('load', dfd.rejectOnError(function (response) {
					assert.deepEqual(response.data.query, { foo: 'bar' });
					assert.ok(!doneFired);
				})),
				notify('stop', dfd.callback(function () {
					assert.ok(doneFired);
				}))
			);

			xhr.get('/__services/request/xhr?foo=bar', { handleAs: 'json' });
		},

		error: function () {
			var dfd = this.async();

			var doneFired = false;
			handle = remover(
				notify('done', dfd.rejectOnError(function (data) {
					doneFired = true;
					assert.instanceOf(data, Error);
				})),
				notify('error', dfd.rejectOnError(function (data) {
					assert.instanceOf(data, Error);
					assert.ok(!doneFired);
				})),
				notify('stop', dfd.callback(function () {
					assert.ok(doneFired);
				}))
			);

			xhr.get('doesntExist.text');
		},

		'done/stop': function () {
			var dfd = this.async(),
				doneCount = 0,
				stopCount = 0,
				stopCalledAfterDone = false;

			handle = remover(
				notify('done', function () {
					doneCount++;
					stopCalledAfterDone = stopCount === 0;
				}),
				notify('stop', function () {
					stopCount++;
				})
			);

			all([
				xhr.get('/__services/request/xhr?delay=1000'),
				xhr.get('/__services/request/xhr?delay=1000')
			]).then(
				dfd.callback(function () {
					assert.ok(stopCalledAfterDone);
					assert.strictEqual(doneCount, 2);
					assert.strictEqual(stopCount, 1);
				}),
				dfd.reject
			);
		},

		cancel: function () {
			var dfd = this.async();

			handle = remover(
				notify('send', function (response, cancel) {
					cancel();
				})
			);

			xhr.get('/__services/request/xhr?delay=1000').then(
				dfd.reject,
				dfd.callback(function (error) {
					assert.instanceOf(error, CancelError);
				})
			);
		}
	});
});
