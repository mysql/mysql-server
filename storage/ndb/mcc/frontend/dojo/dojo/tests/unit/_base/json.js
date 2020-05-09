define([
	'intern!object',
	'intern/chai!assert',
	'../../../_base/json'
], function (registerSuite, assert, json) {
	registerSuite({
		name: 'dojo/_base/json',
		'toJson and fromJson': function () {
			// Not testing dojo.toJson() on its own since Rhino will output the object properties in a different order.
			// Still valid json, but just in a different order than the source string.

			// take a json-compatible object, convert it to a json string, then put it back into json.
			var testObj = {
				a: 'a',
				b: 1,
				c: 'c',
				d: 'd',
				e: {
					e1: 'e1',
					e2: 2
				},
				f: [1, 2, 3],
				g: 'g',
				h: {
					h1: { h2: { h3: 'h3' }}
				},
				i: [[0, 1, 2], [3], [4]]
			},
			obj = json.fromJson(json.toJson(testObj));

			assert.deepEqual(obj, testObj);

			var badJson;
			assert.throws(function () {
				badJson = json.fromJson('bad json'); // should throw an exception, and not set badJson
			});

			assert.isUndefined(badJson);
		},

		'dojo extended json': function () {
			var testObj = {
				ex1: {
					b: 3,
					json: function () {
						return 'json' + this.b;
					}
				},
				ex2: {
					b: 4,
					__json__: function () {
						return '__json__' + this.b;
					}
				}
			},
			testStr = json.toJson(testObj);
			assert.equal('{"ex1":"json3","ex2":"__json__4"}', testStr);
		},

		'pretty print json': function () {
			if (typeof JSON === 'undefined') { // only test our JSON stringifier
				var testObj = {array: [1, 2, { a: 4, b: 4 }]};
				var testStr = json.toJson(testObj, true);
				assert.equal('{\n\t\"array\": [\n\t\t1,\n\t\t2,\n\t\t{\n\t\t\t\"a\": 4,\n\t\t\t\"b\": 4\n\t\t}\n\t]\n}', testStr);
			}
		},

		'eval json': function () {
			var testStr = '{func: function(){}, number: Infinity}';
			var testObj = json.fromJson(testStr);

			assert.isFunction(testObj.func);
			assert.isNumber(testObj.number);
		},

		'toJson called on string': function () {
			assert.equal('"hello"', json.toJson('hello'));
		}
	});
});
