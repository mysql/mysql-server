define([
	'intern!object',
	'intern/chai!assert',
	'../../../store/Observable',
	'../../../_base/declare',
	'dojo/_base/lang',
	'dojo/_base/array',
	'../../../store/Memory',
	'sinon'
], function (registerSuite, assert, Observable, declare, lang, array, Memory, sinon) {
	function createMyStore() {
		var MyStore = declare([ Memory ], {
			get: function () {
				// need to make sure that this.inherited still works with Observable
				return this.inherited(arguments);
			}
		});

		var storeData = [
			{ id: 0, name: 'zero', even: true, prime: false },
			{ id: 1, name: 'one', prime: false },
			{ id: 2, name: 'two', even: true, prime: true },
			{ id: 3, name: 'three', prime: true },
			{ id: 4, name: 'four', even: true, prime: false },
			{ id: 5, name: 'five', prime: true }
		];

		return new Observable(new MyStore({ data: storeData }));
	}

	function createBigStore() {
		var data = [], i;
		for (i = 1; i <= 100; i++) {
			data.push({ id: i, name: 'item ' + i, order: i });
		}

		/* jshint newcap:false */
		return Observable(new Memory({ data: data }));
	}

	registerSuite({
		name: 'dojo/store/Observable',

		'.get': function () {
			var store = createMyStore();
			assert.equal(store.get(1).name, 'one');
			assert.equal(store.get(4).name, 'four');
			assert.isTrue(store.get(5).prime);
		},

		'.query': {
			'empty query options': function () {
				var store = createMyStore();
				var results = store.query({});
				assert.lengthOf(results, 6);
			}
		},

		'.observe': (function () {
			var store, handlerStub, results, observer;

			return {
				beforeEach: function () {
					store = createMyStore();
					handlerStub = sinon.stub();
					results = store.query({ prime: true });
					observer = results.observe(handlerStub);
				},

				'handler receives updates': {
					'update an existing matching record to no longer match query': function () {
						var record = results[0];

						record.prime = false;
						store.put(record);
						assert.deepEqual(handlerStub.firstCall.args[0], {
							id: 2,
							name: 'two',
							even: true,
							prime: false
						});
						assert.equal(handlerStub.firstCall.args[1], 0);
						assert.equal(handlerStub.firstCall.args[2], -1);
						assert.lengthOf(results, 2);
					},

					'updating an existing non-matching record to match the query': function () {
						var record = store.get(1);

						record.prime = true;
						store.put(record);
						assert.deepEqual(handlerStub.firstCall.args[0], {
							id: 1,
							name: 'one',
							prime: true
						});
						assert.equal(handlerStub.firstCall.args[1], -1);
						assert.equal(handlerStub.firstCall.args[2], 3);
						assert.lengthOf(results, 4);
					},

					'does not receive updates from non-matching additions': function () {
						assert.lengthOf(results, 3);
						store.add({ id: 6, name: 'six' });
						assert.lengthOf(results, 3);
						assert.isFalse(handlerStub.called);
					},

					'adding new matching record': function () {
						var record = { id: 7, name: 'seven', prime: true };

						store.add(record);
						assert.isTrue(handlerStub.called);
						assert.deepEqual(handlerStub.firstCall.args[0], record);
						assert.equal(handlerStub.firstCall.args[1], -1);
						assert.equal(handlerStub.firstCall.args[2], 3);
						assert.lengthOf(results, 4);
					},

					includeObjectUpdates: {
						beforeEach: function () {
							observer.cancel();
						},

						'is false; does not notify about object updates': function () {
							var record = results[0];

							results.observe(handlerStub, false);
							record.name = 'newName';
							store.put(record);
							assert.equal(handlerStub.callCount, 0);
						},

						'is true, notifies about object updates': function () {
							var record = results[0];

							results.observe(handlerStub, true);
							record.name = 'newName';
							store.put(record);
							assert.equal(handlerStub.callCount, 1);
							assert.equal(handlerStub.firstCall.args[1], handlerStub.firstCall.args[2]);
						}
					}
				},

				'observer#cancel() halts further updates': function () {
					var record = results[0];

					// Assert the expected length before altering the data set
					assert.lengthOf(results, 3);

					// updating the record will change the observed results
					record.prime = false;
					store.put(record);
					assert.lengthOf(results, 2);
					assert.lengthOf(store.query({ prime: true }), 2);
					assert.equal(handlerStub.callCount, 1);

					// cancel observation and the handler is no longer called
					// and the results are no longer updated
					observer.cancel();
					record.prime = true;
					store.put(record);
					assert.lengthOf(results, 2);
					assert.equal(handlerStub.callCount, 1);

					// verify that the change did happen but was not tracked
					assert.lengthOf(store.query({ prime: true }), 3);
				},

				'observer#remove() is observer#cancel()': function () {
					assert.isFunction(observer.cancel);
					assert.equal(observer.cancel, observer.remove);
				}
			};
		})(),

		'behaves as a mixin wrapper': function () {
			assert.notInstanceOf(createMyStore(), Observable);
			assert.notInstanceOf(createBigStore(), Observable);
		},

		'paging tests': function () {
			var options = { count: 25, sort: [{ attribute: 'order' }] };
			var bigStore = createBigStore();
			var results = [
				bigStore.query({}, lang.delegate(options, { start: 0 })),
				bigStore.query({}, lang.delegate(options, { start: 25 })),
				bigStore.query({}, lang.delegate(options, { start: 50 })),
				bigStore.query({}, lang.delegate(options, { start: 75 }))
			];
			var observeHandler = sinon.stub();

			array.forEach(results, function (result) {
				result.observe(observeHandler, true);
			});

			bigStore.add({ id: 101, name: 'one oh one', order: 2.5 });
			assert.lengthOf(results[0], 26);
			assert.lengthOf(results[1], 25);
			assert.lengthOf(results[2], 25);
			assert.lengthOf(results[3], 25);

			bigStore.remove(101);
			assert.equal(observeHandler.callCount, 2);
			assert.lengthOf(results[0], 25);

			bigStore.add({ id: 102, name: 'one oh two', order: 26.5 });
			assert.lengthOf(results[0], 25);
			assert.lengthOf(results[1], 26);
			assert.lengthOf(results[2], 25);
			assert.equal(observeHandler.callCount, 3);
		}
	});
});
