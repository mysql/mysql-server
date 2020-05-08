define([
	'intern!object',
	'intern/chai!assert',
	'../../../request/node',
	'intern/dojo/node!http',
	'intern/dojo/node!url',
	'intern/dojo/Deferred'
], function (registerSuite, assert, request, http, url, Deferred) {
	var serverPort = 8124;
	var serverUrl = 'http://localhost:' + serverPort;
	var server;

	function getRequestUrl(dataKey, httpState){
		return serverUrl + '?dataKey=' + dataKey + '&' + 'httpState=' + httpState;
	}

	registerSuite({
		name: 'dojo/request/node',

		'before': function () {
			var dfd = new Deferred();
			var responseDataMap = {
				'fooBar': '{ "foo": "bar" }',
				'invalidJson': '<not>JSON</not>'
			};
			var httpCodeMap = {
				'error': 500,
				'success': 200
			};

			function getResponseData(request){
				var parseQueryString = true;
				var urlInfo = url.parse(request.url, parseQueryString);
				return responseDataMap[urlInfo.query.dataKey];
			}

			function getHTTPCode(request){
				var parseQueryString = true;
				var urlInfo = url.parse(request.url, parseQueryString);
				return httpCodeMap[urlInfo.query.httpState];
			}

			server = http.createServer(function(request, response){
				var body = getResponseData(request);
				var httpCode = getHTTPCode(request);

				response.writeHead(httpCode, {
					'Content-Length': body.length,
					'Content-Type': 'application/json'
				});
				response.write(body);
				response.end();
			});

			server.on('listening', dfd.resolve);
			server.listen(serverPort);

			return dfd.promise;
		},

		'after': function () {
			server.close();
		},

		'.get': {
			'successfully get a record': function () {
				var dfd = this.async();

				request.get(getRequestUrl('fooBar', 'success'), {
					handleAs: 'json'
				}).then(dfd.callback(function(data){
					assert.deepEqual(data, {foo: 'bar'});
				}), dfd.reject.bind(dfd));
			},

			'invalid json response throws': function () {
				var dfd = this.async();

				request.get(getRequestUrl('invalidJson', 'success'), {
					handleAs: 'json'
				}).then(
					dfd.reject.bind(dfd),
					dfd.callback(function(err){
						assert.instanceOf(err, SyntaxError);
				}));
			},

			'http error state throws': function () {
				var dfd = this.async();

				request.get(getRequestUrl('fooBar', 'error'), {
					handleAs: 'json'
				}).then(
					dfd.reject.bind(dfd),
					dfd.resolve
				);
			}
		}
	});
});
