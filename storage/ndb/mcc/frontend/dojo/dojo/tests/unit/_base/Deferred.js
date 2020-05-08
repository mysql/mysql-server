define([
	'intern!object',
	'intern/chai!assert',
	'intern/dojo/_base/kernel',
	'dojo/when',
	'testing/_base/Deferred'
], function (
	registerSuite,
	assert,
	kernel,
	when,
	Deferred
) {
	function delay(ms) {
		var d = new Deferred();
		ms = ms || 20;
		setTimeout(function () {
			d.progress(0.5);
		}, ms / 2);
		setTimeout(function () {
			d.resolve();
		}, ms);
		return d.promise;
	}

	registerSuite({
		name: 'dojo/_base/Deferred',

		'.callback()': function () {
			var dfd = new Deferred();
			dfd.addCallback(this.async().callback(function (res) {
				assert.equal(res, 5);
			}));
			dfd.callback(5);
		},

		'.callback() with args': function () {
			var dfd = new Deferred();
			dfd.addCallback(kernel.global, this.async().callback(function (base, res) {
				assert.equal(base + res, 35);
			}), 30);
			dfd.callback(5);
		},

		'.errBack()': function () {
			var dfd = new Deferred();
			dfd.addErrback(this.async().callback(function () {
				assert.ok(true);
			}));
			dfd.errback();
		},

		'.callback() twice': function () {
			var dfd = new Deferred();
			var cnt = 0;
			var thrown = false;
			dfd.addCallback(function () {
				return ++cnt;
			});
			dfd.callback();
			assert.equal(cnt, 1);
			try {
				dfd.callback();
			} catch (e) {
				thrown = true;
			}
			assert.isTrue(thrown);
		},

		'.addBoth()': function () {
			var dfd = new Deferred();
			dfd.addBoth(this.async().callback(function () {
				assert.ok(true);
			}));
			dfd.callback();
		},

		'.addCallback() nested': function () {
			var dfd = new Deferred();
			dfd.addCallback(function () {
				dfd.addCallback(this.async().callback(function (res2) {
					assert.equal('blue', res2);
				}));
				return 'blue';
			});
			dfd.callback('red');
		},

		'.then() simple': function () {
			delay().then(this.async().callback(function () {
				assert.ok(true);
			}));
		},

		'.then() chaining': function () {
			var p = delay();
			var p2 = p.then(function () {
				return 1;
			});
			var p3 = p2.then(function () {
				return 2;
			});
			p3.then(function () {
				p2.then(function (v1) {
					p3.then(this.async().callback(function (v2) {
						assert.equal(v1, 1);
						assert.equal(v2, 2);
					}));
				});
			});
		},

		'when() simple': function () {
			when(delay(), this.async().callback(function () {
				assert.ok(true);
			}));
		},

		'progress': function () {
			var percentDone;
			when(delay(), this.async().callback(function () {
				assert.equal(percentDone, 0.5);
			}), function () {},
			function (completed) {
				percentDone = completed;
			});
		},

		'.cancel() then derivative': function () {
			var def = new Deferred();
			var def2 = def.then();
			var hasThrown = false;
			try {
				def2.cancel();
			} catch (e) {
				hasThrown = true;
			}

			assert.isFalse(hasThrown);
		},

		'.cancel() promise value': function () {
			var cancelledDef;
			var def = new Deferred(function (_def) {
				cancelledDef = _def;
			});
			def.promise.cancel();
			assert.equal(def, cancelledDef);
		},

		'.cancel() doesn\'t write to console': function () {
			var testDef = new Deferred();

			require(['dojo/has', 'testing/Deferred', 'testing/promise/instrumentation'],
				function(has,
						 NewDeferred,
						 instrumentation) {

					var origUseDeferredInstrumentation = has('config-useDeferredInstrumentation');
					has.add('config-useDeferredInstrumentation', "report-unhandled-rejections", null, true);
					instrumentation(NewDeferred);
					require(['testing/promise/instrumentation'], function () {
						var def = new Deferred(),
						errOrig = console.error,
						errorThrown = false;

						console.error = function (s) {
							if(s == "CancelError"){
								console.error = errOrig;
								testDef.reject(s);
							}
							errOrig.apply(null, arguments);
						}

						setTimeout(function() {
							if (!testDef.isCanceled() && !testDef.isFulfilled()) {
								testDef.resolve();
							}
							has.add('config-useDeferredInstrumentation', origUseDeferredInstrumentation, null, true);
							instrumentation(NewDeferred);
						}, 2500);

						def.cancel();
					}
				);
			});

			return testDef.promise;
		},

		'.error() result': function () {
			var def = new Deferred();
			var result = new Error('rejected');
			def.reject(result);
			assert.equal(def.fired, 1);
			assert.equal(def.results[1], result);
		},

		'global leak': function () {
			var def = new Deferred();
			def.then(this.async().callback(function () {
				assert.isUndefined(kernel.global.results, 'results is leaking into global');
				assert.isUndefined(kernel.global.fired, 'fired is leaking into global');
			}));
			def.resolve(true);
		},

		'back and forth process': function () {
			var def = new Deferred();

			def.addErrback(function () {
				return 'ignore error and throw this good string';
			}).addCallback(function () {
				throw new Error('error1');
			}).addErrback(function () {
				return 'ignore second error and make it good again';
			}).addCallback(this.async().callback(function () {
				assert.ok(true);
			}));

			def.errback('');
		},

		'back and forth process then': function () {
			var def = new Deferred();

			def.then(null, function () {
				return 'ignore error and throw this good string';
			}).then(function () {
				throw 'error1';
			}).then(null, function () {
				return 'ignore second error and make it good again';
			}).then(this.async().callback(function () {
				assert.ok(true);
			}));

			def.reject('');
		},

		'return error object': function () {
			var def = new Deferred();

			def.addCallback(function () {
				return new Error('returning an error should work same as throwing');
			}).addErrback(this.async().callback(function () {
				assert.ok(true);
			}));

			def.callback();
		},

		'return error object then': function () {
			var def = new Deferred();

			def.then(function () {
				return new Error('returning an error should NOT work same as throwing');
			}).then(this.async().callback(function () {
				assert.ok(true);
			}));

			def.resolve();
		},

		'errBack with promise': function () {
			var def = new Deferred();

			def.addCallbacks(function () {}, function (err) {
				return err;
			});
			def.promise.then(null, this.async().callback(function () {
				assert.ok(true);
			}));
			def.errback(new Error());
		},

		'test dojo promise progress basic': function () {
			var a = new Deferred();
			var b = new Deferred();
			var called = false;

			a.then(function () {
				b.then(function () {
					if (!called) {
						console.log('Boo. ProgressBasic not called');
					}
				}, function () {
					console.log('Unexpected');
				}, this.async().callback(function () {
					called = true;
					assert.ok(called);
				}));
			});

			a.resolve();
			b.progress();
			b.resolve();
		},

		'test dojo promise progress chain': function () {
			var a = new Deferred();
			var b = new Deferred();
			var called = false;

			a.then(function () {
				return b;
			}).then(function () {
				if (!called) {
					console.log('Boo. ProgressChain not called');
				}
			}, function () {
				console.log('Unexpected');
			}, this.async().callback(function () {
				called = true;
				assert.ok(called);
			}));

			a.resolve();
			b.progress();
			b.resolve();
		}

	});
});

