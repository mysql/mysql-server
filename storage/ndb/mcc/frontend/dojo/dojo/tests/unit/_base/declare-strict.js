/* globals global */
(function (global) {
	define([
		'intern!object',
		'intern/chai!assert',
		'dojo/aspect',
		'dojo/has',
		'dojo/_base/lang',
		'../../../_base/declare',
		'sinon'
	], function (
		registerSuite,
		assert,
		aspect,
		has,
		lang,
		declare,
		sinon
	) {
		'use strict';

		function hasStrict() {
			return !this;
		}

		// IE prior to 9 does not handle Named Function Expressions correctly, so no code that uses
		// NFEs should be run in old IE
		// http://kangax.github.io/nfe/#jscript-bugs
		function hasNfeBug() {
			return has('ie') < 9;
		}

		var nfeMessage = "Old IE does not support NFEs correctly";

		registerSuite({
			name: 'dojo/_base/declare in strict mode',

			setup: function () {
				global.hasStrictModeSupport = hasStrict();
			},

			teardown: function () {
				global.hasStrictModeSupport = undefined;
				global.tests = undefined;
			},

			// There is a bug in lang.setObject() which prevents declare from extending a
			// global which has been set to undefined. To work around this problem we
			// are setting global.tests to an empty object before each test and once all
			// the tests have completed we set it to undefined.
			beforeEach: function () {
				global.tests = {};
			},

			afterEach: function () {
				global.testsFoo = undefined;
			},

			'inherited explicit call': function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				var foo = 'xyzzy';
				var test;

				declare('tests._base.declare.tmp14', null, {
					foo: 'thonk',
					bar: function (arg1, arg2) {
						if (arg1) {
							this.foo = arg1;
						}
						if (arg2) {
							foo = arg2;
						}
					}
				});

				declare('tests._base.declare.tmp15', global.tests._base.declare.tmp14, {
					constructor: function () {
						this.foo = 'blah';
					},
					bar: function bar(arg1, arg2) {
						this.inherited('bar', bar, arguments, [arg2, arg1]);
					},
					baz: function () {
						global.tests._base.declare.tmp15.superclass.bar.apply(this, arguments);
					}
				});

				test = new global.tests._base.declare.tmp15();
				assert.equal(test.foo, 'blah');
				assert.equal(foo, 'xyzzy');

				test.baz('zot');
				assert.equal(test.foo, 'zot');
				assert.equal(foo, 'xyzzy');

				test.bar('trousers', 'squiggle');
				assert.equal(test.foo, 'squiggle');
				assert.equal(foo, 'trousers');
			},

			'inherited with mixin calls': function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				var test;

				declare('tests._base.declare.tmp16', null, {
					foo: '',
					bar: function () {
						this.foo += 'tmp16';
					}
				});

				declare('tests._base.declare.mixin16', null, {
					bar: function bar() {
						this.inherited(bar, arguments);
						this.foo += '.mixin16';
					}
				});

				declare('tests._base.declare.mixin17', global.tests._base.declare.mixin16, {
					bar: function bar() {
						this.inherited(bar, arguments);
						this.foo += '.mixin17';
					}
				});

				declare('tests._base.declare.tmp17', [
					global.tests._base.declare.tmp16,
					global.tests._base.declare.mixin17
				], {
					bar: function bar() {
						this.inherited(bar, arguments);
						this.foo += '.tmp17';
					}
				});

				test = new global.tests._base.declare.tmp17();
				test.bar();
				assert.equal(test.foo, 'tmp16.mixin16.mixin17.tmp17');
			},

			'basic mixin': function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				// testing if a plain Class-like object can be inherited
				// by declare
				var test;

				function Thing() { }

				Thing.prototype.method = sinon.spy();

				declare('tests.Thinger', Thing, {
					method: function method() {
						this.inherited(method, arguments);
					}
				});

				test = new global.tests.Thinger();
				test.method();
				assert.isTrue(Thing.prototype.method.called, 'expected method to be called');
			},

			'mutated methods': function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				// testing if methods can be mutated (within a reason)
				declare('tests._base.declare.tmp18', null, {
					constructor: function () { this.clear(); },
					clear: function () { this.flag = 0; },
					foo: function () { ++this.flag; },
					bar: function () { ++this.flag; },
					baz: function () { ++this.flag; }
				});

				declare('tests._base.declare.tmp19', global.tests._base.declare.tmp18, {
					foo: function foo() { ++this.flag; this.inherited(foo, arguments); },
					bar: function bar() { ++this.flag; this.inherited(bar, arguments); },
					baz: function baz() { ++this.flag; this.inherited(baz, arguments); }
				});

				var x = new global.tests._base.declare.tmp19();
				// smoke tests
				assert.equal(x.flag, 0);

				x.foo();
				assert.equal(x.flag, 2);

				x.clear();
				assert.equal(x.flag, 0);

				var a = 0;

				// aspect.after() on a prototype method
				aspect.after(global.tests._base.declare.tmp19.prototype, 'foo', function () { a = 1; });
				x.foo();
				assert.equal(x.flag, 2);
				assert.equal(a, 1);
				x.clear();
				a = 0;

				// extra chaining
				var old = global.tests._base.declare.tmp19.prototype.bar;
				global.tests._base.declare.tmp19.prototype.bar = function () {
					a = 1;
					++this.flag;
					old.call(this);
				};

				x.bar();
				assert.equal(x.flag, 3);
				assert.equal(a, 1);
				x.clear();
				a = 0;

				// replacement
				global.tests._base.declare.tmp19.prototype.baz = function baz() {
					a = 1;
					++this.flag;
					this.inherited('baz', baz, arguments);
				};

				x.baz();
				assert.equal(x.flag, 2);
				assert.equal(a, 1);
			},

			'modified instance': function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				var stack;

				declare('tests._base.declare.tmp20', null, {
					foo: function () { stack.push(20); }
				});

				declare('tests._base.declare.tmp21', null, {
					foo: function foo() {
						this.inherited(foo, arguments);
						stack.push(21);
					}
				});

				declare('tests._base.declare.tmp22', global.tests._base.declare.tmp20, {
					foo: function foo() {
						this.inherited(foo, arguments);
						stack.push(22);
					}
				});

				declare('tests._base.declare.tmp23', [
					global.tests._base.declare.tmp20,
					global.tests._base.declare.tmp21
				], {
					foo: function foo() {
						this.inherited(foo, arguments);
						stack.push(22);
					}
				});

				var a = new global.tests._base.declare.tmp22();
				var b = new global.tests._base.declare.tmp23();
				var c = {
					foo: function foo() {
						this.inherited('foo', foo, arguments);
						stack.push('INSIDE C');
					}
				};

				stack = [];
				a.foo();
				assert.deepEqual(stack, [20, 22]);

				stack = [];
				b.foo();
				assert.deepEqual(stack, [20, 21, 22]);

				lang.mixin(a, c);
				lang.mixin(b, c);

				stack = [];
				a.foo();
				assert.deepEqual(stack, [20, 22, 'INSIDE C']);

				stack = [];
				b.foo();
				assert.deepEqual(stack, [20, 21, 22, 'INSIDE C']);
			},

			safeMixin: function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				var C = declare(null, {
					foo: sinon.spy()
				});
				var c = new C();
				// make sure we can mixin foo
				declare.safeMixin(c, {
					foo: function foo() {
						this.inherited(foo, arguments);
					}
				});
				sinon.spy(c, 'foo');
				c.foo();

				assert.isTrue(C.prototype.foo.called);
				assert.isTrue(c.foo.called);
				assert.doesNotThrow(function () {
					declare.safeMixin(c);
				});
			},

			'throws error': function () {
				var A = declare(null, {
					foo: function () {
						return 'foo';
					}
				});

				var B = declare(A, {
					foo: function () {
						return this.inherited(arguments);
					}
				});

				var b = new B();

				if (global.hasStrictModeSupport) {
					assert.throws(function () {
						b.foo();
					}, 'strict mode inherited', 'Calling inherited without callee parameter should throw error');
				}
				else {
					assert.strictEqual(b.foo(), 'foo');
				}
			},

			getInherited: function () {
				if (hasNfeBug()) {
					this.skip(nfeMessage);
				}

				var A = declare(null, {
					foo: function () {
						return 'foo';
					}
				});

				var B = declare(A, {
					foo: function foo() {
						var inheritedMethod = this.getInherited(foo, arguments);

						return inheritedMethod() + 'bar';
					}
				});

				var b = new B();

				assert.strictEqual(b.foo(), 'foobar');
			},

			'getInherited throws error': function () {
				var A = declare(null, {
					foo: function () {
						return 'foo';
					}
				});

				var B = declare(A, {
					foo: function () {
						return this.getInherited(arguments)();
					}
				});

				var b = new B();

				if (global.hasStrictModeSupport) {
					assert.throws(function () {
						b.foo();
					}, 'strict mode inherited', 'Calling getInherited without callee parameter should throw error');
				}
				else {
					assert.strictEqual(b.foo(), 'foo');
				}
			}
		});
	});
})(typeof global !== 'undefined' ? global : this);
