define([
	'intern!object',
	'intern/chai!assert',
	'dojo/_base/array',
	'../../hash',
	'../../router/RouterBase'
], function (registerSuite, assert, arrayUtil, hash, RouterBase) {

	// This test uses RouterBase so that we can test a few different
	// behaviors of the router which require re-initializing a new router
	var count = 0, a, b, testObject, test,
		prevented = false, goResult, routeHit,
		router, handle;

	// Simple helper to make tearDown simpler
	function removeAll(handles) {
		arrayUtil.forEach(handles, function (handle) {
			handle.remove();
		});
	}

	function registerRoute(route) {
		return router.register(route, function () {
			count++;
		});
	}

	registerSuite({
		name: 'dojo/router',

		after: function () {
			// reset the hash when everything is done
			hash('');
		},

		'existence': {
			before: function () {
				hash('', true);
				router = new RouterBase();
			},
			'methods': function () {
				assert.ok(router.register, 'Router has a register');
				assert.ok(router.go, 'Router has a go');
				assert.ok(router.startup, 'Router has a startup');
				assert.ok(router.destroy, 'Router has a destroy');
			}
		},
		'register': {
			before: function () {
				hash('');
				count = 0;
				router = new RouterBase();
			},
			after: function () {
				removeAll(handle);
				count = 0;
				// NOTE: router.destroy should be fixed to handle
				// calling destroy without calling started, without erring
				if (router._started) {
					router.destroy();
				}
			},
			'handle': function () {
				handle = registerRoute('/foo');
				assert.ok(handle.remove, 'Handle has a remove');
				assert.ok(handle.register, 'Handle has a register');
			}
		},
		'events': {
			beforeEach: function () {
				hash('');
				count = 0;
				router = new RouterBase();
			},
			afterEach: function () {
				removeAll(handle);
				count = 0;
				if (router._started) {
					router.destroy();
				}
			},
			'before startup': function () {
				handle = registerRoute('/foo');
				hash('/foo');
				assert.strictEqual(count, 0, 'Count should have been 0, was ' + count);
			},
			'after startup': function () {
				handle = registerRoute('/foo');
				router.startup('/foo');
				assert.strictEqual(count, 1, 'Count should have been 1, was ' + count);
			},
			'change route': function () {
				handle = [];
				handle.push(registerRoute('/foo'));
				router.startup('/foo');
				var dfd = this.async();
				handle.push(router.register('/bar', dfd.callback(function () {
					count++;
					assert.strictEqual(count, 2, 'Count should have been 2, was ' + count);
				})));
				hash('/bar');
			},
			'go': function () {
				handle = [];
				handle.push(registerRoute('/foo'));
				handle.push(registerRoute('/bar'));
				router.startup('/foo');
				router.go('/bar');
				router.go('/foo');
				assert.strictEqual(count, 3, 'Count should have been 3, was ' + count);
			},
			'remove': function () {
				handle = registerRoute('/foo');
				router.startup('');
				handle.remove();
				router.go('/foo');
				assert.strictEqual(count, 0, 'Count should have been 0, was ' + count);
			},
			'regex': function () {
				router.startup('');
				handle = registerRoute(/^\/bar$/);
				router.go('/bar');
				assert.strictEqual(count, 1, 'Count should have been 1, was ' + count);
			}
		},
		'event object': {
			before: function () {
				hash('');
				count = 0;
				router = new RouterBase();
				router.startup('');
			},
			after: function () {
				removeAll(handle);
				count = 0;
				if (router._started) {
					router.destroy();
				}
			},
			'structure': function () {
				var oldPath, newPath, params, stopImmediatePropagation, preventDefault;
				handle = router.register('/checkEventObject/:foo', function (event) {
					oldPath = event.oldPath;
					newPath = event.newPath;
					params = event.params;
					stopImmediatePropagation = event.stopImmediatePropagation;
					preventDefault = event.preventDefault;
				});
				router.go('/checkEventObject/bar');

				assert.strictEqual(oldPath, '', 'oldPath should be empty string, was ' + oldPath);
				assert.strictEqual(newPath, '/checkEventObject/bar', 'newPath should be /checkEventObject/bar, was ' + newPath);
				assert.ok(params, 'params should be a truthy value, was ' + params);
				assert.property(params, 'foo', 'params should have a .foo property');
				assert.strictEqual(params.foo, 'bar', 'params.foo should be bar, was ' + params.foo);
				assert.isFunction(stopImmediatePropagation, 'stopImmediatePropagation should be a function, was ' + stopImmediatePropagation);
				assert.isFunction(preventDefault, 'preventDefault should be a function, was ' + preventDefault);
			}
		},
		'route arguments' : {
			beforeEach: function () {
				hash('');
				router = new RouterBase();
				router.startup('');
			},
			afterEach: function () {
				removeAll(handle);
				if (router._started) {
					router.destroy();
				}
			},
			'string route': function () {
				a = b = null;
				handle = router.register('/stringtest/:applied/:arg', function (event, applied, arg) {
					a = applied;
					b = arg;
				});
				router.go('/stringtest/extra/args');

				assert.strictEqual(a, 'extra', 'a should have been extra, was ' + a);
				assert.strictEqual(b, 'args', 'b should have been args, was ' + b);
			},
			'regex route': function () {
				a = b = null;
				handle = router.register(/\/regextest\/(\w+)\/(\w+)/, function (event, applied, arg) {
					a = applied;
					b = arg;
				});
				router.go('/regextest/extra/args');

				assert.strictEqual(a, 'extra', 'a should have been extra, was ' + a);
				assert.strictEqual(b, 'args', 'b should have been args, was ' + b);
			},
			'string route, long with placeholders': function () {
				testObject = null;
				handle = router.register('/path/:to/:some/:long/*thing', function (event) {
					testObject = event.params;
				});
				router.go('/path/to/some/long/thing/this/is/in/splat');

				assert.isObject(testObject, 'testObject should have been an object');
				assert.strictEqual(testObject.to, 'to', 'testObject.to should have been to, was ' + testObject.to);
				assert.strictEqual(testObject.some, 'some', 'testObject.some should have been some, was ' + testObject.some);
				assert.strictEqual(testObject['long'], 'long', 'testObject.long should have been long, was ' + testObject['long']);
				assert.strictEqual(testObject.thing, 'thing/this/is/in/splat', 'testObject.thing should have been thing/this/is/in/splat, was ' + testObject.thing);
			},
			'string route, numerical with placeholders': function () {
				testObject = null;
				handle = router.register('/path/:to/:some/:long/*thing', function (event) {
					testObject = event.params;
				});
				router.go('/path/1/2/3/4/5/6');

				assert.isObject(testObject, 'testObject should have been an object');
				assert.strictEqual(testObject.to, '1', 'testObject.to should have been 1, was ' + testObject.to);
				assert.strictEqual(testObject.some, '2', 'testObject.some should have been 2, was ' + testObject.some);
				assert.strictEqual(testObject['long'], '3', 'testObject.long should have been 3, was ' + testObject['long']);
				assert.strictEqual(testObject.thing, '4/5/6', 'testObject.thing should have been 4/5/6, was ' + testObject.thing);
			}
		},
		'route arguments, capture groups': {
			beforeEach: function () {
				testObject = null;
				hash('');
				router = new RouterBase();
				router.startup('');
				handle = router.register(/^\/path\/(\w+)\/(\d+)$/, function (event) {
					testObject = event.params;
				});
			},
			afterEach: function () {
				removeAll(handle);
				if (router._started) {
					router.destroy();
				}
			},
			'full match': function () {
				router.go('/path/abcdef/1234');

				assert.isArray(testObject, 'testObject should have been an array');
				assert.strictEqual(testObject[0], 'abcdef', 'testObject[0] should have been abcdef, was ' + testObject[0]);
				assert.strictEqual(testObject[1], '1234', 'testObject[1] should have been 1234, was ' + testObject[1]);
			},
			'no match': function () {
				router.go('/path/abc/def');

				assert.ok(!testObject, 'testObject should have been null');
			},
			'no match alternate': function () {
				router.go('/path/abc123/def');

				assert.ok(!testObject, 'testObject should have been null');
			},
			'full match, alternate': function () {
				router.go('/path/abc123/456');

				assert.isArray(testObject, 'testObject should have been an array');
				assert.strictEqual(testObject[0], 'abc123', 'testObject[0] should have been abc123, was ' + testObject[0]);
				assert.strictEqual(testObject[1], '456', 'testObject[1] should have been 456, was ' + testObject[1]);
			}
		},
		'order and propagation': {
			beforeEach: function () {
				hash('');
				test = '';
				handle = [];
				router = new RouterBase();
				router.startup('');
			},
			afterEach: function () {
				removeAll(handle);
				if (router._started) {
					router.destroy();
				}
			},
			'.registerBefore': function () {
				handle.push(router.register('/isBefore', function () {
					test += '1';
				}));
				handle.push(router.registerBefore('/isBefore', function () {
					test += '2';
				}));
				handle.push(router.register('/isBefore', function () {
					test += '3';
				}));
				handle.push(router.registerBefore('/isBefore', function () {
					test += '4';
				}));
				handle.push(router.register('/isBefore', function () {
					test += '5';
				}));
				router.go('/isBefore');

				assert.strictEqual(test, '42135', 'test should have been 42135, was ' + test);
			},
			'.stopImmediatePropagation': function () {
				handle.push(router.register('/stopImmediatePropagation', function () {
					test += 'A';
				}));
				handle.push(router.register('/stopImmediatePropagation', function () {
					test += 'B';
				}));
				handle.push(router.register('/stopImmediatePropagation', function (event) {
					event.stopImmediatePropagation();
					test += 'C';
				}));
				handle.push(router.register('/stopImmediatePropagation', function () {
					test += 'D';
				}));
				handle.push(router.register('/stopImmediatePropagation', function () {
					test += 'E';
				}));
				router.go('/stopImmediatePropagation');

				assert.strictEqual(test, 'ABC', 'test should have been ABC, was ' + test);
			}
		},
		'defaults': {
			before: function () {
				hash('');
				handle = [];
				router = new RouterBase();
			},
			after: function () {
				removeAll(handle);
				if (router._started) {
					router.destroy();
				}
			},
			'.preventDefault': function () {
				prevented = false, goResult = false;
				router.startup('');
				assert.strictEqual(hash(), '', 'hash should be empty');

				handle.push(router.register('/preventDefault', function (event) {
					event.preventDefault();
				}));
				handle.push(router.register('/allowDefault', function () {
					// not preventing default
				}));
				goResult = router.go('/preventDefault');

				assert.strictEqual(hash(), '', 'hash should still be empty');
				assert.ok(!goResult, 'goResult should be false');

				goResult = router.go('/someOtherPath');

				assert.strictEqual(hash(), '/someOtherPath', 'hash should be /someOtherPath');
				assert.ok(goResult, 'goResult should be true');
			},
			'default path': function () {
				routeHit = false;
				handle = router.register('/default', function () {
					routeHit = true;
				});
				router.startup('/default');

				assert.ok(!routeHit, 'Our route was not hit, but should have been');
			}
		}
	});
});
