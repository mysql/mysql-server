define([
	'intern!object',
	'intern/chai!assert',
	'../../aspect',
	'sinon'
], function (registerSuite, assert, aspect, sinon) {
	var slice = Array.prototype.slice;

	function createBeforeSpy() {
		return sinon.spy(function (a) {
			return [a + 1];
		});
	}

	registerSuite(function () {
		var obj;
		var methodSpy;

		return {
			name: 'dojo/aspect',

			'beforeEach': function () {
				methodSpy = sinon.spy(function (a) {
					return a + 1;
				});
				obj = { method: methodSpy };
			},

			'.before': {
				'return value passed as arguments': function () {
					var aspectSpy = createBeforeSpy();

					aspect.before(obj, 'method', aspectSpy);

					obj.method(0);
					assert.isTrue(aspectSpy.calledBefore(methodSpy));
					assert.isTrue(aspectSpy.calledOnce);
					assert.isTrue(methodSpy.calledOnce);
					assert.equal(aspectSpy.lastCall.args[0], 0);
					assert.equal(methodSpy.lastCall.args[0], 1);
					assert.equal(methodSpy.returnValues[0], 2);
				},

				'multiple aspect.before()': function () {
					var aspectSpy1 = createBeforeSpy();
					var aspectSpy2 = createBeforeSpy();

					aspect.before(obj, 'method', aspectSpy1);
					aspect.before(obj, 'method', aspectSpy2);

					obj.method(5);
					assert.isTrue(aspectSpy2.calledBefore(aspectSpy1));
					assert.isTrue(aspectSpy1.calledBefore(methodSpy));
					assert.equal(aspectSpy2.lastCall.args[0], 5);
					assert.equal(aspectSpy1.lastCall.args[0], 6);
					assert.equal(methodSpy.lastCall.args[0], 7);
					assert.equal(methodSpy.returnValues[0], 8);
				},

				'multiple aspect.before() with removal inside handler': function () {
					var count = 0;

					var handle1 = aspect.before(obj, 'method', function () {
						count++;
					});

					var handle2 = aspect.before(obj, 'method', function () {
						count++;
						handle2.remove();
						handle1.remove();
					});

					assert.doesNotThrow(function () {
						obj.method();
					});
					assert.strictEqual(count, 1, 'Only one advising function should be called');
				}
			},

			'.after': {
				'multiple dojo version': function() {
					var aspectAfterCount = 0;
					require({
						packages: [
							{ name: 'dojo1', location: '.' },
							{ name: 'dojo2', location: '.' }
						]
					}, ['dojo1/aspect', 'dojo2/aspect'], this.async().callback(function(aspectOne, aspectTwo) {
						//empty function to aspect on
						var target = {};
						target.onclick = function() {};

						aspectOne.after(target, 'onclick', function() {
							aspectAfterCount++;
						});
						aspectTwo.after(target, 'onclick', function() {
							aspectAfterCount++;
						});
						aspectTwo.after(target, 'onclick', function() {
							aspectAfterCount++;
						});

						target.onclick();

						assert.equal(aspectAfterCount, 3);
					}));
				},
				'overriding return value from original method': function () {
					var expected = 'override!';
					var aspectSpy = sinon.stub().returns(expected);

					aspect.after(obj, 'method', aspectSpy);
					assert.equal(obj.method(0), expected);
					assert.isTrue(aspectSpy.calledAfter(methodSpy));
				},

				'multiple aspect.after()': function () {
					var aspectStub1 = sinon.stub();
					var aspectStub2 = sinon.stub();

					aspect.after(obj, 'method', aspectStub1);
					aspect.after(obj, 'method', aspectStub2);

					obj.method(0);
					assert.isTrue(aspectStub1.calledAfter(methodSpy));
					assert.isTrue(aspectStub2.calledAfter(aspectStub1));
				},

				'multiple aspect.after() with removal inside handler': function () {
					var count = 0;

					var handle1 = aspect.after(obj, 'method', function () {
						handle1.remove();
						handle2.remove();
						count++;
					});

					var handle2 = aspect.after(obj, 'method', function () {
						count++;
					});

					assert.doesNotThrow(function () {
						obj.method();
					});
					assert.strictEqual(count, 1, 'Only one advising function should be called');
				},

				'recieveArguments is true': {
					'provides the original arguments to the aspect method': function () {
						var expected = 'expected';
						var aspectStub = sinon.stub().returns(expected);

						aspect.after(obj, 'method', aspectStub);
						assert.equal(obj.method(0), expected);
						assert.isTrue(aspectStub.calledAfter(methodSpy));
						assert.equal(aspectStub.lastCall.args[0], 1);
						assert.deepEqual(slice.call(aspectStub.lastCall.args[1]), methodSpy.lastCall.args);
					},

					'not overriding return value': function () {
						var aspectStub = sinon.stub().returns(undefined);

						aspect.after(obj, 'method', aspectStub, true);
						assert.equal(obj.method(0), 1);
						assert.isTrue(aspectStub.calledAfter(methodSpy));
					}
				}
			},

			'.around': {
				'single around': function () {
					var expected = 5;
					var aroundFunction = sinon.stub().returns(expected);
					var aspectStub = sinon.stub().returns(aroundFunction);

					aspect.around(obj, 'method', aspectStub);

					assert.equal(obj.method(0), expected);
					assert.isTrue(aspectStub.calledOnce);
					assert.isTrue(aroundFunction.calledOnce);
					assert.equal(aroundFunction.firstCall.args[0], 0);
					assert.isFalse(methodSpy.called);

					// test that the original method was provided
					aspectStub.callArgWith(0, 10);
					assert.isTrue(methodSpy.calledOnce);
					assert.equal(methodSpy.firstCall.args[0], 10);
				}
			},

			'handle.remove()': {
				'prevents aspect from being called': function () {
					var aspectSpy = createBeforeSpy();
					var handle = aspect.before(obj, 'method', aspectSpy);

					obj.method(0);
					assert.notEqual(obj.method, methodSpy);

					handle.remove();
					obj.method(1);
					assert.notEqual(obj.method, methodSpy);
					assert.isTrue(methodSpy.calledTwice);
					assert.isTrue(aspectSpy.calledOnce);
				},

				'can remove an aspect from the middle of a list': function () {
					var aspectSpy1 = createBeforeSpy();
					var aspectSpy2 = createBeforeSpy();
					var handle = aspect.before(obj, 'method', aspectSpy1);

					aspect.before(obj, 'method', aspectSpy2);
					handle.remove();

					obj.method(0);
					assert.isTrue(methodSpy.called);
					assert.isTrue(aspectSpy2.called);
					assert.isFalse(aspectSpy1.called);
				},

				'removing a aspect stub': function () {
					var obj = {};
					var aspectSpy = sinon.stub();
					aspect.before(obj, 'method', sinon.stub());
					var handle = aspect.before(obj, 'method', aspectSpy);

					handle.remove();
					obj.method(0);
					assert.isFalse(aspectSpy.called);
				},

				'removing the first of multiple aspects': function () {
					var aroundFunction = sinon.stub();
					var aspectStub = sinon.stub().returns(aroundFunction);
					var handle = aspect.around(obj, 'method', aspectStub);

					handle.remove();
					obj.method(0);
					assert.isTrue(aspectStub.calledOnce);
					assert.isTrue(methodSpy.calledOnce);
					assert.isFalse(aroundFunction.called);
				}
			}
		};
	});
});
