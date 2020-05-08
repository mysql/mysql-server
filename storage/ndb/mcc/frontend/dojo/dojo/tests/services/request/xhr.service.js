define([
	'require',
	'../util',
	'intern/dojo/node!querystring',
	'intern/dojo/node!fs'
], function (require, util, qs, fs) {
	function xml() {
		return {
			status: 200,
			headers: {
				'Content-Type': 'application/xml'
			},
			body: [
				'<?xml version="1.0" encoding="UTF-8" ?>',
				'<foo><bar baz="thonk">blargh</bar><bar>blah</bar></foo>'
			]
		};
	}

	function responseType(filename, mimeType) {
		return util.call(fs.readFile, filename).then(function (buffer) {
			return {
				status: 200,
				headers: {
					'Content-Type': mimeType
				},
				body: [
					buffer.toString()
				]
			};
		});
	}

	return function (request) {
		var dfd = new util.Promise(function (resolve) {
			function respond(data) {
				resolve({
					status: 200,
					headers: {
						'Content-Type': 'application/json'
					},
					body: [
						JSON.stringify({
							method: request.method,
							query: request.query,
							headers: request.headers,
							url : request.nodeRequest.url,
							payload: data || null
						})
					]
				});
			}

			if (request.serviceURL.indexOf('/xml') > -1) {
				resolve(xml(request));
				return;
			}

			if (request.serviceURL.indexOf('/responseTypeGif') > -1) {
				resolve(responseType(require.toUrl('./support/blob.gif'), 'image/gif'));
				return;
			}

			if (request.serviceURL.indexOf('/responseTypeDoc') > -1) {
				resolve(responseType(require.toUrl('./support/document.html'), 'text/html'));
				return;
			}

			if (request.data) {
				resolve(request.data.then(function (data) {
					return {
						status: 200,
						headers: {
							'Content-Type': 'application/json'
						},
						body: [
							JSON.stringify(data)
						]
					};
				}));
				return;
			}

			if (request.method !== 'GET') {
				request.body.join().then(function (data) {
					respond(qs.parse(data));
				});
			}
			else {
				respond();
			}
		}, true);

		if (request.query.simulateProgress) {
			dfd.progress({ type: 'progress' });
		}

		var milliseconds = request.query.delay;
		if (milliseconds) {
			milliseconds = parseInt(milliseconds, 10);
			dfd.promise = util.delay(dfd.promise, milliseconds);
		}

		return dfd.promise;
	};
});
