define([
	'intern!object',
	'intern/chai!assert',
	'../../../request/xhr',
	'../../../errors/RequestTimeoutError',
	'../../../errors/CancelError',
	'dojo/promise/all',
	'dojo/query',
	'require',
	'../../../has'
], function(registerSuite, assert, xhr, RequestTimeoutError, CancelError, all, query, require, has){
	var global = this;
	var hasFormData = 'FormData' in this && typeof FormData === 'function';
	var hasResponseType = typeof XMLHttpRequest !== 'undefined' &&
		typeof new XMLHttpRequest().responseType !== 'undefined';
	var formData;

	function hasFile(){
		if (typeof File !== 'undefined') {
			try {
				new File();
			} catch (e) {
				// File is not a constructor.
			}
		}
		return false;
	}

	registerSuite({
		name: 'dojo/request/xhr',
		'.get': function(){
			var promise = xhr.get('/__services/request/xhr', {
				preventCache: true,
				handleAs: 'json'
			});

			assert.isFunction(promise.then);
			assert.isFunction(promise.cancel);
			assert.isObject(promise.response);
			assert.isFunction(promise.response.then);
			assert.isFunction(promise.response.cancel);

			return promise.response.then(function(response){
				assert.strictEqual(response.data.method, 'GET');
				assert.strictEqual(response.xhr.readyState, 4);
				return response;
			});
		},

		'.get 404': function(){
			var def = this.async(),
				promise = xhr.get(require.toUrl('./xhr_blarg.html'), {
					preventCache: true
				});

			promise.response.then(
				def.reject,
				def.callback(function(error){
					assert.strictEqual(error.response.status, 404);
				})
			);
		},

		'.get json with truthy value': function(){
			var def = this.async(),
				promise = xhr.get(require.toUrl('./support/truthy.json'), {
					preventCache: true,
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(response){
					assert.strictEqual(response, true);
				})
			);
		},
		'.get json with falsy value': function(){
			var def = this.async(),
				promise = xhr.get(require.toUrl('./support/falsy.json'), {
					preventCache: true,
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(response){
					assert.strictEqual(response, false);
				})
			);
		},

		'.get with progress': function(){
			var def = this.async(),
				promise = xhr.get(require.toUrl('./support/truthy.json'), {
					handleAs: 'json',
					preventCache: true
				});
			promise.then(def.callback(
				function(){
				}), function(error){
				assert.isTrue(false, error);
			}, def.callback(function(event){
				assert.strictEqual(event.transferType, 'download');
			}));
			return def.promise;
		},

		'.get with query': function(){
			var def = this.async(),
				promise = xhr.get('/__services/request/xhr?color=blue', {
					query: {
						foo: ['bar', 'baz'],
						thud: 'thonk',
						xyzzy: 3
					},
					handleAs: 'json'
				});

			promise.response.then(def.callback(function(response){
				assert.strictEqual(response.data.method, 'GET');
				var query = response.data.query;
				assert.ok(query.color && query.foo && query.foo.length && query.thud && query.xyzzy);
				assert.strictEqual(query.color, 'blue');
				assert.strictEqual(query.foo.length, 2);
				assert.strictEqual(query.thud, 'thonk');
				assert.strictEqual(query.xyzzy, '3');
				assert.strictEqual(response.url, '/__services/request/xhr?color=blue&foo=bar&foo=baz&thud=thonk&xyzzy=3');
			}));
		},

		'.post': function(){
			var def = this.async(),
				promise = xhr.post('/__services/request/xhr', {
					data: {color: 'blue'},
					handleAs: 'json'
				});

			promise.response.then(
				def.callback(function(response){
					assert.strictEqual(response.data.method, 'POST');
					var payload = response.data.payload;

					assert.ok(payload && payload.color);
					assert.strictEqual(payload.color, 'blue');
				}),
				def.reject
			);
		},
		'.post ArrayBuffer': function(){
			if (has('native-arraybuffer')) {
				var def = this.async(),
					str = 'foo',
					arrbuff = new ArrayBuffer(str.length),
					i8array = new Uint8Array(arrbuff);

				for (var i = 0; i < str.length; i++) {
					i8array[i] = str.charCodeAt(i);
				}
				var promise = xhr.post('/__services/request/xhr', {
					data: arrbuff,
					handleAs: 'json',
					headers: {
						'Content-Type': 'text/plain'
					}
				});

				promise.response.then(
					def.callback(function(response){
						assert.strictEqual(response.data.method, 'POST');
						var payload = response.data.payload;

						assert.deepEqual(payload, {'foo': ''});
					}),
					def.reject
				);
			} else {
				this.skip('ArrayBuffer not available');
			}
		},
		'.post Blob': function(){
			if (has('native-blob')) {
				var def = this.async(),
					str = 'foo',
					blob = new Blob([str], {type: 'text/plain'});

				var promise = xhr.post('/__services/request/xhr', {
					data: blob,
					handleAs: 'json',
					headers: {
						'Content-Type': 'text/plain'
					}
				});

				promise.response.then(
					def.callback(function(response){
						assert.strictEqual(response.data.method, 'POST');
						var payload = response.data.payload;

						assert.deepEqual(payload, {'foo': ''});
					}),
					def.reject
				);
			} else {
				this.skip('Blob not available');
			}
		},
		'.post File': function(){
			if (hasFile()) {
				var def = this.async(),
					str = 'foo',
					file = new File([str], 'bar.txt', {type: 'text/plain'});

				var promise = xhr.post('/__services/request/xhr', {
					data: file,
					handleAs: 'json',
					headers: {
						'Content-Type': 'text/plain'
					}
				});

				promise.response.then(
					def.callback(function(response){
						assert.strictEqual(response.data.method, 'POST');
						var payload = response.data.payload;

						assert.deepEqual(payload, {'foo': ''});
					}),
					def.reject
				);
			} else {
				this.skip('File or File constructor not available');
			}
		},
		'.post File with upload progress': function(){
			if (hasFile()) {
				var def = this.async(),
					str = 'foo',
					file = new File([str], 'bar.txt', {type: 'text/plain'});

				var promise = xhr.post('/__services/request/xhr', {
					data: file,
					handleAs: 'json',
					uploadProgress: true,
					query: {
						simulateProgress: true
					},
					headers: {
						'Content-Type': 'text/plain'
					}
				});

				promise.then(null, def.reject, def.callback(function(progressEvent){
					assert.isDefined(progressEvent.xhr);
					assert.deepEqual(progressEvent.transferType, 'upload');
				}));
			} else {
				this.skip('File or File constructor not available');
			}
		},
		'.post with query': function(){
			var def = this.async(),
				promise = xhr.post('/__services/request/xhr', {
					query: {
						foo: ['bar', 'baz'],
						thud: 'thonk',
						xyzzy: 3
					},
					data: {color: 'blue'},
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(data){
					assert.strictEqual(data.method, 'POST');
					var query = data.query,
						payload = data.payload;

					assert.ok(query);
					assert.deepEqual(query.foo, ['bar', 'baz']);
					assert.strictEqual(query.thud, 'thonk');
					assert.strictEqual(query.xyzzy, '3');

					assert.ok(payload);
					assert.strictEqual(payload.color, 'blue');
				}),
				def.reject
			);
		},

		'.post string payload': function(){
			var def = this.async(),
				promise = xhr.post('/__services/request/xhr', {
					data: 'foo=bar&color=blue&height=average',
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(data){
					assert.strictEqual(data.method, 'POST');

					var payload = data.payload;

					assert.ok(payload);
					assert.strictEqual(payload.foo, 'bar');
					assert.strictEqual(payload.color, 'blue');
					assert.strictEqual(payload.height, 'average');
				}),
				def.reject
			);
		},

		'.put': function(){
			var def = this.async(),
				promise = xhr.put('/__services/request/xhr', {
					query: {foo: 'bar'},
					data: {color: 'blue'},
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(data){
					assert.strictEqual(data.method, 'PUT');

					assert.ok(data.payload);
					assert.strictEqual(data.payload.color, 'blue');

					assert.ok(data.query);
					assert.strictEqual(data.query.foo, 'bar');
				}),
				def.reject
			);
		},

		'.del': function(){
			var def = this.async(),
				promise = xhr.del('/__services/request/xhr', {
					query: {foo: 'bar'},
					handleAs: 'json'
				});

			promise.then(
				def.callback(function(data){
					assert.strictEqual(data.method, 'DELETE');
					assert.strictEqual(data.query.foo, 'bar');
				}),
				def.reject
			);
		},

		'timeout': function(){
			var def = this.async(),
				promise = xhr.get('/__services/request/xhr', {
					query: {
						delay: '3000'
					},
					timeout: 1000
				});

			promise.then(
				def.reject,
				def.callback(function(error){
					assert.instanceOf(error, RequestTimeoutError);
				})
			);
		},

		cancel: function(){
			var def = this.async(),
				promise = xhr.get('/__services/request/xhr', {
					query: {
						delay: '3000'
					}
				});

			promise.then(
				def.reject,
				def.callback(function(error){
					assert.instanceOf(error, CancelError);
				})
			);
			promise.cancel();
		},

		sync: function(){
			var called = false;

			xhr.get('/__services/request/xhr', {
				sync: true
			}).then(function(){
				called = true;
			});

			assert.ok(called);
		},

		'cross-domain fails': function(){
			var def = this.async();

			xhr.get('http://dojotoolkit.org').response.then(
				def.reject,
				function(){
					def.resolve(true);
				}
			);
		},

		'has Content-Type with data': function(){
			var def = this.async();

			xhr.post('/__services/request/xhr', {
				data: 'testing',
				handleAs: 'json'
			}).then(
				def.callback(function(response){
					assert.equal(response.headers['content-type'], 'application/x-www-form-urlencoded');
				}),
				def.reject
			);
		},

		'no Content-Type with no data': function(){
			var def = this.async();

			xhr.get('/__services/request/xhr', {
				handleAs: 'json'
			}).then(
				def.callback(function(response){
					assert.isUndefined(response.headers['content-type']);
				}),
				def.reject
			);
		},

		headers: function(){
			var def = this.async();

			xhr.get('/__services/request/xhr').response.then(
				def.callback(function(response){
					assert.notEqual(response.getHeader('Content-Type'), null);
				}),
				def.reject
			);
		},

		'custom Content-Type': function(){
			var def = this.async(),
				expectedContentType = 'application/x-test-xhr';

			function post(headers){
				return xhr.post('/__services/request/xhr', {
					query: {
						'header-test': 'true'
					},
					headers: headers,
					data: 'testing',
					handleAs: 'json'
				});
			}

			all({
				lowercase: post({
					'content-type': expectedContentType
				}),
				uppercase: post({
					'CONTENT-TYPE': expectedContentType
				})
			}).then(
				def.callback(function(results){
					assert.match(
						results.lowercase.headers['content-type'],
						/^application\/x-test-xhr(?:;.*)?$/
					);
					assert.match(
						results.uppercase.headers['content-type'],
						/^application\/x-test-xhr(?:;.*)?$/
					);
				}),
				def.reject
			);
		},

		'queryable xml': function(){
			var def = this.async();

			xhr.get('/__services/request/xhr/xml', {
				handleAs: 'xml'
			}).then(
				def.callback(function(xmlDoc){
					var results = query('bar', xmlDoc);

					assert.strictEqual(results.length, 2);
				}),
				def.reject
			);
		},

		'strip fragment': function(){
			var def = this.async(),
				promise = xhr.get('/__services/request/xhr?color=blue#some-hash', {
					handleAs: 'json'
				});

			promise.response.then(def.callback(function(response){
				assert.strictEqual(response.data.method, 'GET');
				assert.strictEqual(response.data.url, '/__services/request/xhr?color=blue');
			}));
		},

		'form data': {
			setup: function(){
				if (!hasFormData) {
					return;
				}

				formData = new FormData();
				formData.append('foo', 'bar');
				formData.append('baz', 'blah');
			},

			post: function(){
				if (!hasFormData) {
					this.skip('No FormData to test');
				}

				var def = this.async();

				xhr.post('/__services/request/xhr/multipart', {
					data: formData,
					handleAs: 'json'
				}).then(
					def.callback(function(data){
						assert.deepEqual(data, {foo: 'bar', baz: 'blah'});
					}),
					def.reject
				);
			},

			teardown: function(){
				formData = null;
			}
		},

		'response type': {
			'Blob': function(){
				if (!hasResponseType) {
					this.skip('No responseType to test');
				}

				return xhr.get('/__services/request/xhr/responseTypeGif', {
					handleAs: 'blob'
				}).then(function(response){
					assert.strictEqual(response.constructor, Blob);
				});
			},

			'Blob POST': function(){
				if (!hasResponseType) {
					this.skip('No responseType to test');
				}

				return xhr.post('/__services/request/xhr/responseTypeGif', {
					handleAs: 'blob'
				}).then(function(response){
					assert.strictEqual(response.constructor, Blob);
				});
			},

			'ArrayBuffer': function(){
				if (has('native-arraybuffer') && hasResponseType) {
					return xhr.get('/__services/request/xhr/responseTypeGif', {
						handleAs: 'arraybuffer'
					}).then(function(response){
						assert.strictEqual(response.constructor, ArrayBuffer);
					});
				} else {
					this.skip('No responseType to test');
				}
			},

			'ArrayBuffer POST': function(){
				if (has('native-arraybuffer') && hasResponseType) {
					return xhr.post('/__services/request/xhr/responseTypeGif', {
						handleAs: 'arraybuffer'
					}).then(function(response){
						assert.strictEqual(response.constructor, ArrayBuffer);
					});
				} else {
					this.skip('No responseType to test');
				}
			},

			'document': function(){
				if (!hasResponseType) {
					this.skip('No responseType to test');
				}

				return xhr.get('/__services/request/xhr/responseTypeDoc', {
					handleAs: 'document'
				}).then(function(response){
					assert.strictEqual(response.constructor, document.constructor);
				});
			},

			'document POST': function(){
				if (!hasResponseType) {
					this.skip('No responseType to test');
				}

				return xhr.post('/__services/request/xhr/responseTypeDoc', {
					handleAs: 'document'
				}).then(function(response){
					assert.strictEqual(response.constructor, document.constructor);
				});
			}
		},

		'Web Workers': {
			'from blob': function(){
				if (!('URL' in global)) {
					this.skip('URL is not supported');
				}

				if (!('Worker' in global)) {
					this.skip('Worker is not supported');
				}

				if (!('Blob' in global)) {
					this.skip('Blob is not supported');
				}

				if (!URL.createObjectURL) {
					this.skip('URL.createObjectURL is not supported');
				}

				var dfd = this.async();
				var baseUrl = location.origin + '/' + require.toUrl('testing');
				var testUrl = location.origin + '/' + require.toUrl('./support/truthy.json');

				var workerFunction = function(){
					self.addEventListener('message', function(event){
						if (event.data.baseUrl) {
							testXhr(event.data.baseUrl, event.data.testUrl);
						}
					});

					dojoConfig = {async: true};

					function testXhr(baseUrl, testUrl){
						var xhr = new XMLHttpRequest();

						dojoConfig.baseUrl = baseUrl;

						xhr.onreadystatechange = function(){
							if (xhr.readyState === 4 && xhr.status === 200) {
								var blob = new Blob([xhr.response], {type: 'application/javascript'});
								var blobURL = URL.createObjectURL(blob);

								importScripts(blobURL);

								require([
									'dojo/request/xhr'
								], function(xhr){
									xhr.get(testUrl).then(function(response){
										if (response === 'true') {
											self.postMessage('success');
										} else {
											throw new Error(response);
										}
									}, function(error){
										throw error;
									});
								});
							}
						};
						xhr.open('GET', baseUrl + '/dojo.js', true);
						xhr.send();
					}
				};

				var blob = new Blob(['(' + workerFunction.toString() + ')()'], {type: 'application/javascript'});
				var worker = new Worker(URL.createObjectURL(blob));

				worker.addEventListener('error', function(error){
					dfd.reject(error);
				});

				worker.addEventListener('message', function(message){
					if (message.data === 'success') {
						dfd.resolve();
					} else {
						dfd.reject(message);
					}
				});

				worker.postMessage({
					baseUrl: baseUrl,
					testUrl: testUrl
				});
			}
		}
	});
});
