define([
	'intern!object',
	'intern/chai!assert',
	'../../../_base/kernel',
	'../../../_base/xhr',
	'../../../_base/Deferred',
	'../../../topic',
	'dojo/_base/array',
	'dojo/query',
	'dojo/json',
	'dojo/dom-construct',
	'dojo/Deferred',
	'dojo/promise/all',
	'dojo/domReady!'
], function (registerSuite, assert, dojo, xhr, baseDeferred, topic,
			 array, query, JSON, domConstruct, Deferred, all) {
	var form, topicCount, remover;
	registerSuite({
		name: 'dojo/_base/xhr',

		'text content handler': function () {
			assert.strictEqual(
				dojo._contentHandlers.text({
					responseText: 'foo bar baz '
				}),
				'foo bar baz '
			);
		},

		'json content handler': function () {
			var object = {
				foo: 'bar',
				baz: [
					{ thonk: 'blarg' },
					'xyzzy!'
				]
			};
			assert.deepEqual(
				dojo._contentHandlers.json({
					responseText: JSON.stringify(object)
				}),
				object
			);
		},

		'json-comment-filtered handler': function () {
			var object = {
				foo: 'bar',
				baz: [
					{ thonk: 'blarg' },
					'xyzzy!'
				]
			};

			assert.throws(function () {
				dojo._contentHandlers['json-comment-filtered']({
					responseText: JSON.stringify(object)
				});
			});
			assert.deepEqual(
				dojo._contentHandlers['json-comment-filtered']({
					responseText: '\tblag\n/*' + JSON.stringify(object) + '*/\n\r\t\r'
				}),
				object
			);
			assert.deepEqual(
				dojo._contentHandlers['json-comment-optional']({
					responseText: '\tblag\n/*' + JSON.stringify(object) + '*/\n\r\t\r'
				}),
				object
			);
		},

		'javascript content handler': function () {
			var object = {
				foo: 'bar',
				baz: [
					{ thonk: 'blarg' },
					'xyzzy!'
				]
			};

			assert.deepEqual(
				dojo._contentHandlers['javascript']({
					responseText: '(' + JSON.stringify(object) + ')'
				}),
				object
			);
			assert.ok(
				dojo._contentHandlers['javascript']({
					responseText: 'true;'
				})
			);
			assert.ok(
				!dojo._contentHandlers['javascript']({
					responseText: 'false;'
				})
			);
		},

		'xml content handler': function () {
			var fakeXHR = {
				responseText: '<foo><bar baz="thonk">blarg</bar></foo>'
			};

			if ('DOMParser' in dojo.global) {
				var parser = new DOMParser();
				fakeXHR.responseXML = parser.parseFromString(fakeXHR.responseText, 'text/xml');
			}

			var xmlDoc = dojo._contentHandlers['xml'](fakeXHR);
			assert.strictEqual(xmlDoc.documentElement.tagName, 'foo');
		},

		'.get': {
			success: function () {
				var dfd = this.async(30000, 2),
					xdfd = xhr.get({
						url: '/__services/request/xhr',
						preventCache: true,
						handleAs: 'json',
						load: dfd.callback(function (data, ioArgs) {
							assert.strictEqual(ioArgs.xhr.readyState, 4);
							return data;
						})
					});

				assert.instanceOf(xdfd, baseDeferred);

				xdfd.addCallback(dfd.callback(function (data) {
					assert.strictEqual(data.method, 'GET');
				}));
				xdfd.addErrback(dfd.reject);
			},

			'404 status': function () {
				var dfd = this.async(30000, 2),
					xdfd = xhr.get({
						url: 'xhr_blarg.html',
						error: dfd.callback(function (err, ioArgs) {
							assert.strictEqual(ioArgs.xhr.status, 404);
							return err;
						})
					});

				xdfd.addCallback(dfd.reject);
				xdfd.addErrback(dfd.resolve);
			},

			content: function () {
				var dfd = this.async(),
					xdfd = xhr.get({
						url: '/__services/request/xhr?color=blue',
						content: {
							foo: [ 'bar', 'baz' ],
							thud: 'thonk',
							xyzzy: 3
						}
					});

				xdfd.addCallback(dfd.callback(function () {
					assert.strictEqual(
						xdfd.ioArgs.url,
						'/__services/request/xhr?color=blue&foo=bar&foo=baz&thud=thonk&xyzzy=3'
					);
				}));
			},

			form: {
				setup: function () {
					form = domConstruct.place('<form id="f3" style="border: 1px solid black;"><input id="f3_spaces" type="hidden" name="spaces" value="string with spaces"></form>', document.body);
				},

				teardown: function () {
					domConstruct.destroy(form);
					form = null;
				},

				'serialize': function () {
					var dfd = this.async(),
						xdfd = xhr.get({
							url: '/__services/request/xhr',
							form: 'f3'
						});

					xdfd.addCallback(dfd.callback(function () {
						assert.strictEqual(
							xdfd.ioArgs.url,
							'/__services/request/xhr?spaces=string%20with%20spaces'
						);
					}));
				},

				'with content': function () {
					var dfd = this.async(),
						xdfd = xhr.get({
							url: '/__services/request/xhr',
							form: 'f3',
							content: { spaces: 'blah' }
						});

					xdfd.addCallback(dfd.callback(function () {
						assert.strictEqual(
							xdfd.ioArgs.url,
							'/__services/request/xhr?spaces=blah'
						);
					}));
				}
			}
		},

		'.post': {
			success: function () {
				var dfd = this.async();

				xhr.post({
					url: '/__services/request/xhr?foo=bar',
					content: { color: 'blue' },
					handleAs: 'json',
					handle: dfd.callback(function (data) {
						assert.strictEqual(data.method, 'POST');
						assert.deepEqual(data.query, { foo: 'bar' });
						assert.deepEqual(data.payload, { color: 'blue' });
					})
				});
			},

			'with content': function () {
				var dfd = this.async(),
					xdfd = xhr.post({
						url: '/__services/request/xhr',
						content: {
							foo: [ 'bar', 'baz' ],
							thud: 'thonk',
							xyzzy: 3
						}
					});

				xdfd.addCallback(dfd.callback(function () {
					assert.strictEqual(xdfd.ioArgs.query, 'foo=bar&foo=baz&thud=thonk&xyzzy=3');
				}));
				xdfd.addErrback(dfd.reject);
			},

			form: {
				setup: function () {
					form = domConstruct.place('<form id="f4" style="border: 1px solid black;" action="/__services/request/xhr"><input id="f4_action" type="hidden" name="action" value="Form with input named action"></form>', document.body);
				},

				teardown: function () {
					domConstruct.destroy(form);
					form = null;
				},

				'serialize': function () {
					var dfd = this.async(),
						xdfd = xhr.post({
							form: 'f4'
						});

					xdfd.addCallback(dfd.resolve);
					xdfd.addErrback(dfd.reject);
				}
			},

			raw: function () {
				var dfd = this.async(),
					xdfd = xhr.post({
						url: '/__services/request/xhr',
						postData: 'foo=bar&color=blue&height=average',
						handleAs: 'json'
					});

				xdfd.addCallback(dfd.callback(function (data) {
					assert.deepEqual(data.payload, {
						foo: 'bar',
						color: 'blue',
						height: 'average'
					});
				}));
				xdfd.addErrback(dfd.reject);
			}
		},

		'.put': function () {
			var dfd = this.async(),
				xdfd = xhr.put({
					url: '/__services/request/xhr?foo=bar',
					content: { color: 'blue' },
					handleAs: 'json'
				});

			xdfd.addCallback(dfd.callback(function (data) {
				assert.strictEqual(data.method, 'PUT');
				assert.deepEqual(data.query, { foo: 'bar' });
				assert.deepEqual(data.payload, { color: 'blue' });
			}));
			xdfd.addErrback(dfd.reject);
		},

		'.del': function () {
			var dfd = this.async(),
				xdfd = xhr.del({
					url: '/__services/request/xhr',
					preventCache: true,
					handleAs: 'json'
				});

			xdfd.addCallback(dfd.callback(function (data) {
				assert.strictEqual(data.method, 'DELETE');
			}));
			xdfd.addErrback(dfd.reject);
		},

		cancel: function () {
			var dfd = this.async(),
				xdfd = xhr.post({
					url: '/__services/request/xhr?delay=3000'
				});

			xdfd.addCallback(dfd.reject);
			xdfd.addErrback(dfd.callback(function (error) {
				assert.instanceOf(error, Error);
				assert.strictEqual(error.dojoType, 'cancel');
			}));

			xdfd.cancel();
		},

		'xml queryable': function () {
			var dfd = this.async(),
				xdfd = xhr.get({
					url: '/__services/request/xhr/xml',
					handleAs: 'xml'
				});

			xdfd.addCallback(dfd.callback(function (xmlDoc) {
				var results = query('bar', xmlDoc);
				assert.strictEqual(results.length, 2);
			}));
			xdfd.addErrback(dfd.reject);
		},

		ioPublish: {
			setup: function () {
				dojo.config.ioPublish = true;
				topicCount = {};

				var handles = [];
				remover = function () {
					array.forEach(handles, function (handle) {
						handle.remove();
					});
					handles = null;
				};

				array.forEach(
					[ 'start', 'send', 'load', 'error', 'done' ],
					function (name) {
						topicCount[name] = 0;
						handles.push(
							topic.subscribe('/dojo/io/' + name, function () {
								topicCount[name] += 1;
							})
						);
					}
				);
			},

			teardown: function () {
				dojo.config.ioPublish = false;
				remover();
			},

			counts: function () {
				var dfd = this.async(),
					dfd1 = new Deferred(),
					xdfd1 = xhr.get({ url: '/__services/request/xhr?delay=1000' }),
					dfd2 = new Deferred(),
					xdfd2 = xhr.get({ url: '/__services/request/xhr?delay=1000' });

				xdfd1.addCallback(dfd1.resolve);
				xdfd1.addErrback(dfd1.reject);
				xdfd2.addCallback(dfd2.resolve);
				xdfd2.addErrback(dfd2.reject);

				all([dfd1, dfd2]).then(
					function () {
						setTimeout(dfd.callback(function () {
							assert.deepEqual(topicCount, {
								start: 1,
								send: 2,
								load: 2,
								error: 0,
								done: 2
							});
						}), 100);
					},
					dfd.reject
				);
			}
		}
	});
});
