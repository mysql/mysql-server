define([
	'intern!object',
	'intern/chai!assert',
	'../../json'
], function (registerSuite, assert, json) {
	registerSuite({
		name: 'dojo/json',

		'.parse': {
			'simple string': function () {
				assert.deepEqual(json.parse('{"foo":"bar"}'), { foo: 'bar' });
			},

			'simple true': function () {
				assert.deepEqual(json.parse('{"foo":true}'), { foo: true });
			},

			'simple false': function () {
				assert.deepEqual(json.parse('{"foo":false}'), { foo: false });
			},

			'simple null': function () {
				assert.deepEqual(json.parse('{"foo":null}'), { foo: null });
			},

			'simple number': function () {
				assert.deepEqual(json.parse('{"foo":3.3}'), { foo: 3.3 });
			},

			'strict string': function () {
				assert.deepEqual(json.parse('{"foo":"bar"}', true), { foo: 'bar' });
			},

			'strict empty string': function () {
				assert.deepEqual(json.parse('{"foo":""}', true), { foo: '' });
			},

			'strict escaped string': function () {
				assert.deepEqual(json.parse('{"foo":"b\\n\\t\\"ar()"}', true), { foo: 'b\n\t\"ar()' });
			},

			'strict true': function () {
				assert.deepEqual(json.parse('{"foo":true}', true), { foo: true });
			},

			'strict false': function () {
				assert.deepEqual(json.parse('{"foo":false}', true), { foo: false });
			},

			'strict null': function () {
				assert.deepEqual(json.parse('{"foo":null}', true), { foo: null });
			},

			'strict number': function () {
				assert.deepEqual(json.parse('{"foo":3.3}', true), { foo: 3.3 });
			},

			'strict negative number': function () {
				assert.deepEqual(json.parse('{"foo":-3.3}', true), { foo: -3.3 });
			},

			'strict negative exponent': function () {
				assert.deepEqual(json.parse('{"foo":3.3e-33}', true), { foo: 3.3e-33 });
			},

			'strict exponent': function () {
				assert.deepEqual(json.parse('{"foo":3.3e33}', true), { foo: 3.3e33 });
			},

			'strict array': function () {
				assert.deepEqual(json.parse('{"foo":[3,true,[]]}', true), { foo: [3, true, []] });
			},

			'bad call throws': function () {
				assert.throws(function () {
					json.parse('{"foo":alert()}', true);
				});
			},

			'bad math throws': function () {
				assert.throws(function () {
					json.parse('{"foo":3+4}', true);
				});
			},

			'bad array index throws': function () {
				assert.throws(function () {
					json.parse('{"foo":"bar"}[3]', true);
				});
			},

			'unquoted object key throws': function () {
				assert.throws(function () {
					json.parse('{foo:"bar"}', true);
				});
			},

			'unclosed array throws': function () {
				assert.throws(function () {
					json.parse('[', true);
				});
			},

			'closing an unopened object literal throws': function () {
				assert.throws(function () {
					json.parse('}', true);
				});
			},

			'malformed array throws': function () {
				assert.throws(function () {
					json.parse('["foo":"bar"]');
				});
			}
		},

		'.stringify': {
			'string': function () {
				assert.equal(json.stringify({'foo': 'bar'}), '{"foo":"bar"}');
			},

			'null': function () {
				assert.equal(json.stringify({'foo': null}), '{"foo":null}');
			},

			'function': function () {
				assert.equal(json.stringify({'foo': function () {}}), '{}');
			},

			'NaN': function () {
				assert.equal(json.stringify({'foo': NaN}), '{"foo":null}');
			},

			'Infinity': function () {
				assert.equal(json.stringify({'foo': Infinity}), '{"foo":null}');
			},

			'date': function () {
				// there is differences in how many decimals of accuracies in seconds in how Dates
				// are serialized between browsers
				assert.match(json.parse(json.stringify({'foo': new Date(1)})).foo, /1970-01-01T00:00:00.*Z/);
			},

			'inherited object': function () {
				function FooBar() {
					this.foo = 'foo';
				}

				FooBar.prototype.bar = 'bar';
				assert.equal(json.stringify(new FooBar()), '{"foo":"foo"}');
			},

			'toJson': function () {
				/* jshint unused:false */
				var obj = {foo: {toJSON: function () {
					return {name: 'value'};
				}}};
				assert.equal(json.stringify(obj), '{"foo":{"name":"value"}}');
			}
		}
	});
});
