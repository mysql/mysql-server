define([
	'require',
	'exports',
	'./util',
	'intern/dojo/errors/CancelError',
	'intern/dojo/node!http',
	'intern/dojo/node!url',
	'intern/dojo/node!querystring',
	'intern/dojo/node!glob',
	'intern/dojo/node!http-proxy',
	'intern/dojo/node!jsgi-node',
	'intern/dojo/node!jsgi-node/jsgi/node',
	'intern/dojo/node!formidable'
], function (require, exports, util, CancelError, http, url, qs, glob, httpProxy, jsgi, node, formidable) {
	var nodeWrapper = node.Node,
		proxy = new httpProxy.RoutingProxy();

	require = (function (require) {
		return function (dep) {
			return new util.Promise(function (resolve) {
				require([ dep ], function (module) {
					resolve(module);
				});
			});
		};
	})(require);

	function Multipart(nextApp) {
		//	summary:
		//		Returns JSGI middleware for handling multi-part forms.
		//		If a multi-part request is detected, a promise will be added
		//		to the request object at the "data" property. It will resolve
		//		to the fields sent in the multi-part request.
		var multipartRE = /^multipart\/form-data;/;

		return function (request) {
			var headers = request.headers;
			if (headers['content-type'] && multipartRE.test(headers['content-type'])) {
				request.data = new util.Promise(function (resolve, reject) {
					nodeWrapper(function (nodeRequest) {
						var parser = new formidable.IncomingForm();
						parser.parse(nodeRequest, function (err, fields, files) {
							if (err) {
								reject(err);
							}
							for (var key in files) {
								fields[key] = files[key];
							}
							resolve(fields);
						});
					})(request);
				});
			}

			return nextApp(request);
		};
	}

	exports.start = function (port) {
		return util.call(glob, '**/*.service.js', {
			cwd: './tests/services'
		}).then(function (files) {
			return util.map(files, function (filename) {
				return require('./' + filename.slice(0, -3)).then(function (module) {
					var name = filename.slice(0, -11);
					return {
						regexp: new RegExp('^\/' + name.replace(/\//g, '\\/') + '(\\/.*)?$'),
						module: module
					};
				});
			});
		}).then(function (services) {
			var serviceRE = /^\/__services(\/.*)$/,
				servicesHandler = new jsgi.Listener(new Multipart(function (request) {
					var urlInfo = url.parse(request.pathInfo, true),
						query = qs.parse(request.queryString),
						pathInfo = urlInfo.pathname.match(serviceRE),
						serviceURL = pathInfo && pathInfo[1],
						response;

					if (serviceURL) {
						request.urlInfo = urlInfo;
						request.query = query;
						request.serviceURL = serviceURL;

						services.some(function (service) {
							var match = serviceURL.match(service.regexp);
							if (match) {
								request.urlInfo.pathname = match[1];
								response = service.module.call(null, request);
								return true;
							}
						});
					}

					if (!response) {
						return {
							status: 500,
							headers: {
								'Content-Type': 'text/html;charset=utf-8'
							},
							body: [
								'<!DOCTYPE html><html><head><title>No services</title></head>' +
								'<body>No services</body></html>'
							]
						};
					}
					else if (typeof response.then === 'function') {
						return response.then(function (data) {
							return data;
						}, function (error) {
							if (error instanceof CancelError) {
								// Handle any canceled promises, like from a canceled request
								return {
									body: [ '' ]
								};
							}
							throw error;
						});
					}
					return response;
				}));
			var server = http.createServer(function (request, response) {
				if (request.url.indexOf('/__services/') === 0) {
					servicesHandler(request, response);
				}
				else {
					proxy.proxyRequest(request, response, {
						host: 'localhost',
						port: 9000
					});
				}
			});
			server.listen(port || 9001);

			return server;
		});
	};
});
