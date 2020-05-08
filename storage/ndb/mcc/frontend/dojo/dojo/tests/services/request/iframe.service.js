define([
	'../util',
	'intern/dojo/node!querystring'
], function (util, qs) {
	function xml() {
		return {
			status: 200,
			headers: {
				'Content-Type': 'text/xml'
			},
			body: [
				'<?xml version="1.0" encoding="UTF-8"?>\n',
				'<Envelope title="Test of dojo.io.iframe xml test">\n',
					'<Children>\n',
						'<child>FOO</child>\n',
						'<child>BAR</child>\n',
						'<child>BAZ</child>\n',
						'<child>BAT</child>\n',
					'</Children>\n',
					'<![CDATA[\n',
						'function(){\n',
							'for(var i=0; i<somethign; i++){\n',
								'if(foo>bar){ /* whatever */ }\n',
							'}\n',
						'}\n',
					']]>\n',
					'<a href="something">something else</a>\n',
				'</Envelope>'
			]
		};
	}

	function html(data) {
		return {
			status: 200,
			headers: {
				'Content-Type': 'text/html'
			},
			body: [
				'<html>',
				'<head></head>',
				'<body>',
				data,
				'</body>',
				'</html>'
			]
		};
	}

	function textarea(data) {
		return '<textarea style="width:100%; height: 100px;">' + data + '</textarea>';
	}

	return function (request) {
		var promise = new util.Promise(function (resolve) {
			function respond(data) {
				if (request.query.type === 'html') {
					data = '<h1>SUCCESSFUL HTML response</h1>';
				}
				else {
					if (request.query.type === 'json') {
						data = JSON.stringify({
							method: request.method,
							query: request.query,
							payload: data
						});
					}
					else if (request.query.type === 'javascript') {
						data = 'window.iframeTestingFunction = function(){ return 42; };';
					}
					else if (request.query.text) {
						data = request.query.text;
					}
					else {
						data = 'iframe succeeded';
					}
					data = textarea(data);
				}
				resolve(html(data));
			}

			if (request.query.type === 'xml') {
				resolve(xml());
				return;
			}

			if (request.data) {
				request.data.then(respond);
				return;
			}

			if (request.method !== 'GET') {
				request.body.join().then(function (data) {
					data = qs.parse(data);
					respond(data);
				});
				return;
			}

			respond();
		});

		var milliseconds = request.query.delay;
		if (milliseconds) {
			milliseconds = parseInt(milliseconds, 10);
			promise = util.delay(promise, milliseconds);
		}

		return promise;
	};
});
