define([
	'intern!object',
	'intern/chai!assert',
	'../../../_base/array'
], function (registerSuite, assert, array) {
	registerSuite({
		name: 'dojo/_base/array',

		'.indexOf': {
			'no index' : function () {
				var foo = [128, 256, 512];
				var bar = ['aaa', 'bbb', 'ccc'];
				assert.strictEqual(array.indexOf([45, 56, 85], 56), 1);
				assert.strictEqual(array.indexOf([Number, String, Date], String), 1);
				assert.strictEqual(array.indexOf(foo, foo[1]), 1);
				assert.strictEqual(array.indexOf(foo, foo[2]), 2);
				assert.strictEqual(array.indexOf(bar, bar[1]), 1);
				assert.strictEqual(array.indexOf(bar, bar[2]), 2);
				assert.strictEqual(array.indexOf({a:1}, 'a'), -1);

				foo.push(bar);
				assert.strictEqual(array.indexOf(foo, bar), 3);
			},

			'from index': function () {
				assert.strictEqual(array.indexOf([45, 56, 85], 56, 2), -1);
				assert.strictEqual(array.indexOf([45, 56, 85], 56, 1), 1);
				assert.strictEqual(array.indexOf([45, 56, 85], 56, -3), 1);
				// Make sure going out of bounds doesn't throw us in an infinite loop
				assert.strictEqual(array.indexOf([45, 56, 85], 56, 3), -1);
			}
		},

		'.lastIndexOf': {
			'no index': function () {
				var foo = [128, 256, 512];
				var bar = ['aaa', 'bbb', 'aaa', 'ccc'];

				assert.strictEqual(array.indexOf([45, 56, 85], 56), 1);
				assert.strictEqual(array.indexOf([Number, String, Date], String), 1);
				assert.strictEqual(array.lastIndexOf(foo, foo[1]), 1);
				assert.strictEqual(array.lastIndexOf(foo, foo[2]), 2);
				assert.strictEqual(array.lastIndexOf(bar, bar[1]), 1);
				assert.strictEqual(array.lastIndexOf(bar, bar[2]), 2);
				assert.strictEqual(array.lastIndexOf(bar, bar[0]), 2);
			},

			'from index': function () {
				assert.strictEqual(array.lastIndexOf([45, 56, 85], 56, 1), 1);
				assert.strictEqual(array.lastIndexOf([45, 56, 85], 85, 1), -1);
				assert.strictEqual(array.lastIndexOf([45, 56, 85], 85, -2), -1);
				assert.strictEqual(array.lastIndexOf([45, 56, 45], 45, 0), 0);
			}
		},

		'.forEach': {
			'no exception': function () {
				var foo = [128, 'bbb', 512];
				array.forEach(foo, function (item, index) {
					switch (index) {
						case 0: assert.strictEqual(item, 128); break;
						case 1: assert.strictEqual(item, 'bbb'); break;
						case 2: assert.strictEqual(item, 512); break;
						default: assert.ok(false);
					}
				});
				assert.doesNotThrow(function () {
					array.forEach(undefined, function () {});
				}, /.*/, 'forEach on undefined');
			},

			string: function () {
				var bar = 'abc';
				array.forEach(bar, function (item, index) {
					switch (index) {
						case 0: assert.strictEqual(item, 'a'); break;
						case 1: assert.strictEqual(item, 'b'); break;
						case 2: assert.strictEqual(item, 'c'); break;
						default: assert.ok(false);
					}
				});
			},

			// FIXME: test forEach w/ a NodeList()?
			'string callback': function () {
				// Test using strings as callback", which accept the parameters with
				// the names "item", "index" and "array"!
				var foo = [128, 'bbb', 512];

				// Test that the variable "item" contains the value of each item.
				var obj = {
					_res: ''
				};
				array.forEach(foo, 'this._res += item', obj);
				assert.strictEqual(obj._res, '128bbb512');

				// Test that the variable "index" contains each index.
				obj._res = [];
				array.forEach(foo, 'this._res.push(index)', obj);
				assert.deepEqual(obj._res, [0,1,2]);

				// Test that the variable "array" always contains the entire array.
				obj._res = [];
				array.forEach(foo, 'this._res.push(array)', obj);
				assert.deepEqual(obj._res, [
					[128, 'bbb', 512],
					[128, 'bbb', 512],
					[128, 'bbb', 512]
				]);

				// Catch undefined variable usage (I used to use "i" :-)).
				assert.throws(function () {
					array.forEach(foo, 'this._res += arr[i];', obj);
				}, /.*/, 'forEach on undefined variable');
			}
		},

		'.every': {
			// FIXME: test forEach w/ a NodeList()?
			array: function () {
				var foo = [128, 'bbb', 512];

				assert.ok(
					array.every(foo, function (item, index, array) {
						assert.strictEqual(array.constructor, Array);
						assert.typeOf(array, 'array');
						assert.typeOf(index, 'number');
						if(index === 1) { assert.strictEqual(item, 'bbb'); }
						return true;
					})
				);

				assert.ok(
					array.every(foo, function (item, index) {
						switch (index) {
							case 0: assert.strictEqual(item, 128); return true;
							case 1: assert.strictEqual(item, 'bbb'); return true;
							case 2: assert.strictEqual(item, 512); return true;
							default: return false;
						}
					})
				);

				assert.ok( // intern chai has no assert.notOk
					!array.every(foo, function (item, index) {
						switch (index) {
							case 0: assert.strictEqual(item, 128); return true;
							case 1: assert.strictEqual(item, 'bbb'); return true;
							case 2: assert.strictEqual(item, 512); return false;
							default: return true;
						}
					})
				);

			},

			string: function () {
				var bar = 'abc';
				assert.ok(
					array.every(bar, function (item, index) {
						switch (index) {
							case 0: assert.strictEqual(item, 'a'); return true;
							case 1: assert.strictEqual(item, 'b'); return true;
							case 2: assert.strictEqual(item, 'c'); return true;
							default: return false;
						}
					})
				);

				assert.ok( // intern chai has no assert.notOk
					!array.every(bar, function (item, index) {
						switch (index) {
							case 0: assert.strictEqual(item, 'a'); return true;
							case 1: assert.strictEqual(item, 'b'); return true;
							case 2: assert.strictEqual(item, 'c'); return false;
							default: return true;
						}
					})
				);
			}
		},

		'.some': {
			// FIXME: test NodeList for every()?
			array: function () {
				var foo = [128, 'bbb', 512];
				assert.ok(
					array.some(foo, function (item, index, array) {
						assert.strictEqual(array.length, 3);
						return true;
					})
				);

				assert.ok(
					array.some(foo, function (item, index) {
						return index < 1;

					})
				);

				assert.ok(
					!array.some(foo, function () {
						return false;
					})
				);

				assert.ok(
					array.some(foo, function (item, index, array) {
						assert.strictEqual(array.constructor, Array);
						assert.typeOf(array, 'array');
						assert.typeOf(index, 'number');
						if(index === 1) { assert.strictEqual(item, 'bbb'); }
						return true;
					})
				);
			},

			string: function () {
				var bar = 'abc';
				assert.ok(
					array.some(bar, function (item, index, array) {
						assert.strictEqual(array.length, 3);
						switch (index) {
							case 0: assert.strictEqual(item, 'a'); return true;
							case 1: assert.strictEqual(item, 'b'); return true;
							case 2: assert.strictEqual(item, 'c'); return true;
							default: return false;
						}
					})
				);

				assert.ok(
					array.some(bar, function (item, index) {
						switch (index) {
							case 0: assert.strictEqual(item, 'a'); return true;
							case 1: assert.strictEqual(item, 'b'); return true;
							case 2: assert.strictEqual(item, 'c'); return false;
							default: return true;
						}
					})
				);

				assert.ok(
					!array.some(bar, function () {
						return false;
					})
				);
			}
		},

		'.filter': {
			// FIXME: need to add scoping tests for all of these!!!
			array: function () {
				var foo = ['foo', 'bar', 10];

				assert.deepEqual(
					array.filter(foo, function (item, index) {
						return index < 1;
					}),
					['foo'],
					'index < 1'
				);

				assert.deepEqual(
					array.filter(foo, function (item) {
						return item === 'foo';
					}),
					['foo'],
					'item == "foo"'
				);

				assert.deepEqual(
					array.filter(foo, function () {
						return false;
					}),
					[]
				);

				assert.deepEqual(
					array.filter(foo, function (item) {
						return typeof item === 'number';
					}),
					[10]
				);
			},

			string: function () {
				var foo = 'thinger blah blah blah';
				assert.deepEqual(
					array.filter(foo, function (item, index) {
						return index < 3;
					}),
					['t', 'h', 'i']
				);

				assert.deepEqual(
					array.filter(foo, function () {
						return false;
					}),
					[]
				);
			}
		},

		'.map': function () {
			assert.deepEqual(
				array.map(function () { return true; }, []),
				[]
			);

			assert.deepEqual(
				array.map(['cat', 'dog', 'mouse'], function (item, index) {
					return index+1;
				}),
				[1, 2, 3]
			);
		}
	});
});
