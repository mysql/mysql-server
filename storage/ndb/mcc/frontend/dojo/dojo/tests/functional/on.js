define([
	'require',
	'intern!object',
	'intern/chai!assert'
], function (require, registerSuite, assert) {
	registerSuite({
		name: 'dojo/on',

		'focus normalization': function () {
			return this.get('remote')
				.get(require.toUrl('./on.html'))
				.setExecuteAsyncTimeout(10000)
				.executeAsync(function (send) {
					require([ 'dojo/on', 'dojo/query' ], function (on) {
						var div = document.body.appendChild(document.createElement('div'));
						var input = div.appendChild(document.createElement('input'));
						var focusEventOrder = [];

						on(div, 'input:focusin', function () {
							focusEventOrder.push('in');
						});
						on(div, 'input:focusout', function () {
							focusEventOrder.push('out');
						});

						var otherInput = document.body.appendChild(document.createElement('input'));
						input.focus();
						otherInput.focus();

						setTimeout(function () {
							send(focusEventOrder);
						}, 1);
					});
				})
				.then(function (focusEventOrder) {
					assert.deepEqual(focusEventOrder, [ 'in', 'out' ]);
				});
		}
	});
});
