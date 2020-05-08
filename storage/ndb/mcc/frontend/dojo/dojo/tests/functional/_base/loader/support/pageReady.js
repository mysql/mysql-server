define([
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (pollUntil) {
	return function pageReady(url, config, test) {
		if (typeof config === 'function') {
			test = config;
			config = null;
		}
		var query = config;
		if (typeof config === 'object') {
			query = '';
			for (var key in config) {
				query += (query ? '&' : '') + key + '=' + encodeURIComponent(config[key]);
			}
		}
		if (query) {
			url += '?' + query;
		}

		return function () {
			return test(
				this.get('remote')
					.setExecuteAsyncTimeout(20000)
					.get(url)
					.then(pollUntil(function () {
						return window.ready || null;
					}))
			);
		};
	};
});
