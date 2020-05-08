define([
	'intern!object',
	'intern/chai!assert',
	'../../Stateful',
	'sinon',
	'dojo/_base/declare'
], function (registerSuite, assert, Stateful, sinon, declare) {
	registerSuite({
		name: 'dojo/Stateful',

		'.watch': {
			'notifies watcher when .set()': function () {
				var name = 'foo';
				var stateObj = new Stateful({ foo: 3 });
				var onValueChange = sinon.stub();

				stateObj.watch(name, onValueChange);
				assert.equal(stateObj.get(name), 3);

				stateObj.set(name, 4);
				assert.isTrue(onValueChange.calledOnce);
				assert.deepEqual([name, 3, 4], onValueChange.firstCall.args);
				assert.equal(stateObj.get(name), 4);
			},

			'handle.remove() stops notifications': function () {
				var name = 'foo';
				var stateObj = new Stateful({ foo: 3 });
				var onValueChange = sinon.stub();
				var handle = stateObj.watch(name, onValueChange);

				stateObj.set(name, 4);
				assert.isTrue(onValueChange.calledOnce);

				handle.remove();
				stateObj.set(name, 5);
				assert.isTrue(onValueChange.calledOnce);
			},

			'handle.remove() is reentrant and works correctly with another watch': function () {
				var name = 'foo';
				var stateObj = new Stateful({ foo: 3 });
				var onValueChange1 = sinon.stub();
				var onValueChange2 = sinon.stub();
				var handle = stateObj.watch(name, onValueChange1);

				stateObj.watch(name, onValueChange2);
				stateObj.set(name, 4);
				handle.remove();
				handle.remove();
				stateObj.set(name, 5);

				assert.isTrue(onValueChange1.calledOnce);
				assert.isTrue(onValueChange2.calledTwice);
			},

			'wildcard watcher': function () {
				var stateObj = new Stateful();
				var onAnyValueChange = sinon.stub();
				var onFooValueChange = sinon.stub();

				stateObj.set({ foo: 3, bar: 5 });
				stateObj.watch(onAnyValueChange);
				stateObj.watch('foo', onFooValueChange);

				stateObj.set('foo', 4);
				stateObj.set('bar', 6);

				assert.isTrue(onAnyValueChange.calledTwice);
				assert.isTrue(onFooValueChange.calledOnce);
			},

			'child watcher': function () {
				model = new Stateful({
					user: {
						name: 1
					}
				});

				var results;

				model.watch("user", function(field, oldValue, newValue) {
					newValue.watch(function(_field, _oldValue, _newValue) {
						results = [_field, _oldValue, _newValue];
					});
				});

				var userPet = new Stateful({
					name: 2
				});

				model.set("user", userPet);

				userPet.set("name", 3);

				assert.deepEqual(results, ['name', 2, 3], 'child watcher work correctly');
			}
		},

		'.set': {
			'hashed value': function () {
				var stateObj1 = new Stateful();
				var onValueChange = sinon.stub();
				var handle = stateObj1.watch('foo', onValueChange);
				var stateObj2;

				stateObj1.set({ foo: 3, bar: 5 });
				assert.equal(stateObj1.get('foo'), 3);
				assert.equal(stateObj1.get('bar'), 5);
				assert.isTrue(onValueChange.calledOnce);

				stateObj2 = new Stateful();
				stateObj2.set(stateObj1);
				assert.equal(stateObj2.get('foo'), 3);
				assert.equal(stateObj2.get('bar'), 5);
				// stateObj1 watchers should not be copied to stateObj2
				assert.isTrue(onValueChange.calledOnce);
				handle.unwatch();
			}
		},

		'functional getters and setters': {
			'accessor usage': function () {
				var ExtendedStateful = declare([Stateful], {
					foo: '',
					bar: 0,
					baz: '',

					_fooSetter: function (value) {
						this.foo = value;
					},

					_fooGetter: function () {
						return 'bar';
					},

					_barSetter: function (value) {
						this.bar = value;
					}
				});
				var stateObj = new ExtendedStateful();

				stateObj.set('foo', 'nothing');
				stateObj.set('bar', 2);
				stateObj.set('baz', 'bar');

				assert.equal(stateObj.foo, 'nothing', 'attribute set properly');
				assert.equal(stateObj.get('foo'), 'bar', 'getter working properly');
				assert.equal(stateObj.bar, 2, 'attribute set properly');
				assert.equal(stateObj.get('bar'), 2, 'getter working properly');
				assert.equal(stateObj.baz, 'bar', 'property set properly');
				assert.equal(stateObj.get('baz'), 'bar', 'getter working properly');
			},

			'parameter handling': function () {
				var ExtendedStateful = declare([Stateful], {
					foo: null,
					bar: 5,

					_fooSetter: function (value) {
						this.foo = value;
					},

					_barSetter: function (value) {
						this.bar = value;
					}
				});
				var stateObj = new ExtendedStateful({
					foo: function () {
						return 'baz';
					},
					bar: 4
				});

				assert.typeOf(stateObj.foo, 'function');
				assert.equal(stateObj.foo(), 'baz');
				assert.equal(stateObj.get('bar'), 4);
			},

			'_changeAttrValue() inside a setter': function () {
				var onFooValueChange = sinon.stub();
				var onBarValueChange = sinon.stub();
				var ExtendedStateful = declare([Stateful], {
					foo: null,
					bar: null,

					_fooSetter: function (value) {
						this._changeAttrValue('bar', value);
						this.foo = value;
					},

					_barSetter: function (value) {
						this._changeAttrValue('foo', value);
						this.bar = value;
					}
				});
				var stateObj = new ExtendedStateful();

				stateObj.watch('foo', onFooValueChange);
				stateObj.watch('bar', onBarValueChange);

				stateObj.set('foo', 3);
				assert.equal(stateObj.get('bar'), 3);

				stateObj.set('bar', 4);
				assert.equal(stateObj.get('foo'), 4);

				assert.isTrue(onFooValueChange.calledTwice);
				assert.isTrue(onBarValueChange.calledTwice);
				assert.deepEqual(['bar', null, 3], onBarValueChange.firstCall.args);
				assert.deepEqual(['foo', null, 3], onFooValueChange.firstCall.args);
				assert.deepEqual(['foo', 3, 4], onFooValueChange.secondCall.args);
				assert.deepEqual(['bar', 3, 4], onBarValueChange.secondCall.args);
			},

			'serialize correctly with setters': function () {
				var ExtendedStateful = declare([Stateful], {
					foo: null,

					_fooSetter: function (value) {
						this.foo = value + 'baz';
					}
				});
				var stateObj = new ExtendedStateful({
					foo: 'bar'
				});

				assert.equal(JSON.stringify(stateObj), '{"foo":"barbaz"}');
			}
		}
	});
});
