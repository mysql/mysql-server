define([], function () {
	return function (request) {
		var response = {};

		if (request.query.query === 'dojotoolkit') {
			response.url = 'dojotoolkit.org/';
		}
		return {
			status: 200,
			headers: {
				'Content-Type': 'application/json'
			},
			body: [ request.query.callback + '(' + JSON.stringify(response) + ');' ]
		};
	};
});
