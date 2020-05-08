define([], function () {
	var handlers = {
		myecho: function (params) {
			return '<P>' + params[0] + '</P>';
		},
		contentB: function () {
			return '<P>Content B</P>';
		},
		contentC: function () {
			return '<P>Content C</P>';
		},
		add: function (params) {
			return params[0] + params[1];
		}
	};

	return function (request) {
		var input = '';
		return request.body.forEach(function (string) {
			input += string;
		}).then(function () {
			if (!input) {
				return {
					status: 400,
					headers: { 'Content-Type': 'application/json' },
					body: [ JSON.stringify({ error: 'No data provided' }) ]
				};
			}

			try {
				var data = JSON.parse(input);
				var response = { id: data.id, error: null };

				if (data.method === 'triggerRpcError') {
					response.error = 'Triggered RPC Error test';
				}
				else {
					response.result = handlers[data.method](data.params);
				}
				return {
					status: 200,
					headers: {
						'Content-Type': 'application/json'
					},
					body: [ JSON.stringify(response) ]
				};
			}
			catch (error) {
				return {
					status: 400,
					headers: { 'Content-Type': 'application/json' },
					body: [ JSON.stringify({ error: 'Bad data: ' + input }) ]
				};
			}
		});
	};
});
