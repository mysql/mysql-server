define([
	'intern!object',
	'intern/chai!assert',
	'../../../store/DataStore',
	'../../../data/ItemFileReadStore',
	'../../../data/ItemFileWriteStore',
	'dojo/_base/lang'
], function (registerSuite, assert, DataStore, ItemFileReadStore, ItemFileWriteStore, lang) {
	var data, dataStore, store;

	registerSuite({
		name: 'dojo/store/DataStore',

		beforeEach: function () {
			data = {
				items: [
					{ id: 1, name: 'one', prime: false },
					{ id: 2, name: 'two', even: true, prime: true },
					{ id: 3, name: 'three', prime: true },
					{ id: 4, name: 'four', even: true, prime: false },
					{ id: 5, name: 'five', prime: true, children: [
						{ _reference: 1 },
						{ _reference: 2 },
						{ _reference: 3 }
					] }
				],
				identifier: 'id'
			};

			dataStore = new ItemFileWriteStore({ data: lang.clone(data) });
			dataStore.fetchItemByIdentity({ identity: null });
			store = new DataStore({ store: dataStore });
		},

		construction: {
			'when using a read-only store then write methods unavailable': function () {
				var readOnlyStore = new DataStore({ store: new ItemFileReadStore({}) });
				assert.isNotFunction(readOnlyStore.put);
				assert.isNotFunction(readOnlyStore.add);
			}
		},

		'.get': function () {
			assert.equal(store.get(1).name, 'one');
			assert.equal(store.get(4).name, 'four');
			assert.isTrue(store.get(5).prime);
			assert.equal(store.get(5).children[1].name, 'two');
			assert.ok(typeof store.get(10) === 'undefined');
		},

		'.query': {
			'basic query returns expected data': function () {
				var dfd = this.async(500);
				var query = { even: true };
				var result = store.query(query);

				result.map(dfd.callback(function (record) {
					var expected = data.items[record.id - 1];
					for (var key in record) {
						if (record.hasOwnProperty(key)) {
							assert.propertyVal(expected, key, record[key]);
						}
					}
				}), lang.hitch(dfd, 'reject'));
			},

			'provides children': function () {
				var dfd = this.async(500);
				var query = { prime: true };

				store.query(query).then(dfd.callback(
					function (results) {
						assert.lengthOf(results, 3);
						assert.equal(results[2].children[2].name, 'three');
					}
				), lang.hitch(dfd, 'reject'));
			}
		},

		'.put': {
			'update a record': function () {
				var record = store.get(4);

				assert.notOk(record.square);
				record.square = true;
				store.put(record);
				record = store.get(4);
				assert.isTrue(record.square);
			},

			'add a new record': function () {
				var data = { id: 6, perfect: true };

				store.put(data);
				assert.isTrue(store.get(6).perfect);
			},

			'overwrite new': function () {
				return store.put({
					id: 8,
					name: 'eight'
				}, {
					overwrite: true
				}).then(function () {
					assert.fail();
				}, function () {
					// do nothing... test passes
				});
			}
		},

		'.add': {
			'new record': function () {
				store.add({
					id: 7,
					name: 'seven'
				});
				assert.equal(store.get(7).name, 'seven');
			},

			'existing record': function () {
				return store.add({
					id: 1,
					name: 'one'
				}).then(function () {
					assert.fail();
				}, function () {
					// do nothing... test passes
				});
			}
		},

		'.remove': {
			'multiple calls': function () {
				return store.remove(5).then(function (result) {
					assert.ok(result);
				}).then(function () {
					return store.remove(7);
				}).then(function (result) {
					assert.ok(!result);
				});
			}
		}
	});
});
