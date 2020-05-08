define([
	'intern!object',
	'intern/chai!assert',
	'../../../store/Cache',
	'../../../store/Memory',
	'dojo/_base/array'
], function (registerSuite, assert, Cache, Memory, array) {
	var data, masterStore, cachingStore, store, options;

	registerSuite({
		name: 'dojo/store/Cache',

		beforeEach: function () {
			data = [
				{id: 1, name: 'one', prime: false},
				{id: 2, name: 'two', even: true, prime: true},
				{id: 3, name: 'three', prime: true},
				{id: 4, name: 'four', even: true, prime: false},
				{id: 5, name: 'five', prime: true}
			];
			options = {};
			masterStore = new Memory({ data: data });
			cachingStore = new Memory();
			/* jshint newcap:false */
			store = Cache(masterStore, cachingStore, options);
		},

		'.get': {
			'basic usage': function () {
				array.forEach(data, function (record) {
					assert.equal(store.get(record.id).name, record.name);
				});
			},

			'caches record on first get': function () {
				array.forEach(data, function (record) {
					assert.isUndefined(cachingStore.get(record.id));
					assert.equal(store.get(record.id).name, record.name);
					assert.equal(cachingStore.get(record.id).name, record.name);
				});
			}
		},

		'.query': {
			'options#isLoaded()': {
				'returns false .query() does not trigger a cache': function () {
					options.isLoaded = function () { return false; };
					assert.equal(store.query({prime: true}).length, 3);
					assert.equal(store.query({even: true})[1].name, 'four');
					array.forEach(data, function (record) {
						assert.isUndefined(cachingStore.get(record.id));
					});
				},

				'returns true .query() does cache': function () {
					options.isLoaded = function () { return true; };
					assert.equal(store.query({ prime: true }).length, 3);
					assert.equal(cachingStore.get(3).name, 'three');
				},

				'is undefined caches by default': function () {
					var query = store.query();

					assert.lengthOf(query, 5);
					query.forEach(function(value) {
						assert.equal(cachingStore.get(value.id).name, value.name);
					});
				}
			},

			'with sort': function () {
				assert.equal(store.query({ prime: true }, { sort: [{ attribute: 'name' }] }).length, 3);
				assert.equal(store.query({ even: true }, { sort: [{ attribute: 'name' }] })[1].name, 'two');
			}
		},

		'.put': {
			'updating a record': function () {
				var record = store.get(4);

				assert.isUndefined(record.square);
				record.square = true;
				store.put(record);

				record = store.get(4);
				assert.isTrue(record.square);
				record = cachingStore.get(4);
				assert.isTrue(record.square);
				record = masterStore.get(4);
				assert.isTrue(record.square);
			},

			'creating a new record': function () {
				var record = {
					id: 6,
					perfect: true
				};

				store.put(record);
				assert.isTrue(store.get(6).perfect);
				assert.isTrue(cachingStore.get(6).perfect);
				assert.isTrue(masterStore.get(6).perfect);
			}
		},

		'.add': {
			'adding a new record': function () {
				var record = {
					id: 7,
					prime: true
				};

				store.add(record);
				assert.isTrue(store.get(7).prime);
				assert.isTrue(cachingStore.get(7).prime);
				assert.isTrue(masterStore.get(7).prime);
			},

			'adding an existing record throws': function () {
				var record = {
					id: 5,
					perfect: true
				};

				assert.throws(function () {
					store.add(record);
				});
			},

			'uses masterStore#add()': function () {
				var originalAdd = masterStore.add;
				var result;

				masterStore.add = function () {
					return {
						test: 'value'
					};
				};

				result = store.add({
					id: 7,
					prop: 'doesn\'t matter'
				});
				assert.property(result, 'test');
				assert.equal(result.test, 'value');
				masterStore.add = originalAdd;
			}
		}
	});
});
