define([
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (pollUntil) {
	return function loaderTest(url, config, execute, test) {
		if (typeof config === 'function' || config instanceof Array) {
			test = execute;
			execute = config;
			config = null;
		}
		return function () {
			var command = this.get('remote')
				.setExecuteAsyncTimeout(20000)
				.get(url)
				.execute(function (config) {
					/* global configureLoader */
					if (typeof configureLoader === 'function') {
						configureLoader(config);
					}
				}, [ JSON.stringify(config) ])
				.then(pollUntil(function () {
					return window.ready || null;
				}));

			if (typeof execute === 'function') {
				command = command.executeAsync(execute);
			}
			else {
				command = command.executeAsync.apply(command, execute);
			}
			return command.then(test);
		};
	};
});
