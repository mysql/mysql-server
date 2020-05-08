define([
	'intern!object',
	'intern/chai!assert',
	'../../../store/Memory',
	'dojo/store/util/QueryResults'
], function (registerSuite, assert, MemoryStore, QueryResults) {
	var data = [
		{ id: 1, name: 'one',	even: false,	prime: false,	mappedTo: 'E', date: new Date(1970, 0, 1) },
		{ id: 2, name: 'two',	even: true,		prime: true,	mappedTo: 'D', date: new Date(1980, 1, 2) },
		{ id: 3, name: 'three',	even: false,	prime: true,	mappedTo: 'C', date: new Date(1990, 2, 3) },
		{ id: 4, name: 'four',	even: true,		prime: false,	mappedTo: null, date: new Date(1972, 3, 6, 12, 1) },
		{ id: 5, name: 'five',	even: false,	prime: true,	mappedTo: 'A', date: new Date(1972, 3, 6, 6, 2) }
	];

	var store = new MemoryStore({
		data: data
	});

	registerSuite({
		name: 'dojo/store/Memory',

		'.get': [
			function () {
				assert.strictEqual(store.get(1).name, 'one');
				assert.strictEqual(store.get(4).name, 'four');
				assert.ok(store.get(5).prime);
			}
		],

		'.query': {
			'with boolean': function () {
				assert.strictEqual(store.query({ prime: true }).length, 3);
				assert.strictEqual(store.query({ even: true })[1].name, 'four');
			},

			'with string': function () {
				assert.strictEqual(store.query({ name: 'two' }).length, 1);
				assert.strictEqual(store.query({ name: 'two' })[0].name, 'two');
			},

			'with RegExp': function () {
				assert.strictEqual(store.query({ name: /^t/ }).length, 2);
				assert.strictEqual(store.query({ name: /^t/ })[1].name, 'three');
				assert.strictEqual(store.query({ name: /^o/ }).length, 1);
				assert.strictEqual(store.query({ name: /o/ }).length, 3);
			},

			'with test function': function () {
				var lowIdItems = store.query({ id: {
					test: function (id) {
						return id < 4;
					}
				}});

				assert.strictEqual(lowIdItems.length, 3);

				var evenItems = store.query({ even: {
					test: function (even, object) {
						return even && object.id > 2;
					}
				}});

				assert.strictEqual(evenItems.length, 1);
			},

			'with sort': function () {
				function queryDeepEqual(actual, expected) {
					if (!actual.hasOwnProperty('map')) {
						assert.deepEqual(actual, expected);
						return;
					}
					// When there's no native forEach, map, or filter
					// it gets added to the object and the functions are
					// not equal to each other, so we can't test directly
					// for deep equality :(

					assert.strictEqual(actual.length, expected.length, 'Length of query should be ' + expected.length);
					assert.strictEqual(actual.total, expected.total, 'Total of query should be ' + expected.total);

					for (var i = 0, l = expected.length; i < l; i++) {
						assert.deepEqual(actual[i], expected[i]);
					}
				}
				var sortedByMappedTo = new QueryResults([data[4], data[2], data[1], data[0], data[3]]),
					evenSortedByName = new QueryResults([data[3], data[1]]);

				assert.strictEqual(store.query({ prime: true }, { sort: [ { attribute: 'name' } ] }).length, 3);
				queryDeepEqual(store.query({ even: true }, { sort: [ { attribute: 'name' } ] }), evenSortedByName);

				queryDeepEqual(store.query({ even: true }, { sort: function (a, b) {
					return a.name < b.name ? -1 : 1;
				}}), evenSortedByName);

				queryDeepEqual(store.query(null, { sort: [ { attribute: 'mappedTo' } ] }), sortedByMappedTo);

				queryDeepEqual(store.query({}, { sort: [ { attribute: 'date', descending: false } ] }).map(function (item) {
					return item.id;
				}), new QueryResults([ 1, 5, 4, 2, 3 ]));
			},

			'with paging': function () {
				assert.strictEqual(store.query({ prime: true }, { start: 1, count: 1 }).length, 1);
				assert.strictEqual(store.query({ even: true }, { start: 1, count: 1 })[0].name, 'four');
			}
		},

		'.put': {
			update: function () {
				var four = store.get(4);
				four.square = true;
				store.put(four);
				four = store.get(4);
				assert.ok(four.square);
			},

			'new': function () {
				store.put({
					id: 6,
					perfect: true
				});
				assert.ok(store.get(6).perfect);
			}
		},

		'.add': {
			duplicate: function () {
				var threw;

				try {
					store.add({
						id: 6,
						perfect: true
					});
				} catch (error) {
					threw = true;
				}

				assert.ok(threw);
			},

			'new': function () {
				store.add({
					id: 7,
					prime: true
				});

				assert.ok(store.get(7).prime);
			},

			'new id assignment': function () {
				var object = {
					random: true
				};

				store.add(object);
				assert.isDefined(object.id);
			}
		},

		'.remove': {
			'existing item': function () {
				assert.ok(store.remove(7));
				assert.ok(!store.get(7));
			},

			'missing item': function () {
				assert.ok(!store.remove(77));
				// make sure nothing changed
				assert.strictEqual(store.get(1).id, 1);
			}
		},

		misc: {
			'IFRS style data': function () {
				var anotherStore = new MemoryStore({
					data: {
						items: [
							{ name: 'one',		even: false,	prime: false },
							{ name: 'two',		even: true,		prime: true },
							{ name: 'three',	even: false,	prime: true }
						],
						identifier: 'name'
					}
				});

				assert.strictEqual(anotherStore.get('one').name, 'one');
				assert.strictEqual(anotherStore.query({ name: 'one' })[0].name, 'one');
			}
		}
	});
});
