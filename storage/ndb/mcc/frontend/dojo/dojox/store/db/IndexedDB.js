define(['dojo/_base/declare', 'dojo/_base/lang', 'dojo/Deferred', 'dojo/when', 'dojo/promise/all', 'dojo/store/util/SimpleQueryEngine', 'dojo/store/util/QueryResults'],
	function(declare, lang, Deferred, when, all,  SimpleQueryEngine, QueryResults){

	function makePromise(request) {
		var deferred = new Deferred();
		request.onsuccess = function(event) {
			deferred.resolve(event.target.result);
		};
		request.onerror = function() {
			request.error.message = request.webkitErrorMessage;
			deferred.reject(request.error);
		};
		return deferred.promise;
	}

	// we keep a queue of cursors, so we can prioritize the traversal of result sets
	var cursorQueue = [];
	var maxConcurrent = 1;
	var cursorsRunning = 0;
	var wildcardRe = /(.*)\*$/;
	function queueCursor(cursor, priority, retry) {
		// process the cursor queue, possibly submitting a cursor for continuation
		if (cursorsRunning || cursorQueue.length) {
			// actively processing
			if (cursor) {
				// add to queue
				cursorQueue.push({cursor: cursor, priority: priority, retry: retry});
				// keep the queue in priority order
				cursorQueue.sort(function(a, b) {
					return a.priority > b.priority ? 1 : -1;
				});
			}
			if (cursorsRunning >= maxConcurrent) {
				return;
			}
			var cursorObject = cursorQueue.pop();
			cursor = cursorObject && cursorObject.cursor;
		}//else nothing in the queue, just shortcut directly to continuing the cursor
		if (cursor) {
			try {
				// submit the continuation of the highest priority cursor
				cursor['continue']();
				cursorsRunning++;
			} catch(e) {
				if ((e.name === 'TransactionInactiveError' || e.name === 0) && cursorObject) { // == 0 is IndexedDBShim
					// if the cursor has been interrupted we usually need to create a new transaction, 
					// handing control back to the query/filter function to open the cursor again
					cursorObject.retry();
				} else {
					throw e;
				}
			}
		}
	}
	function yes(){
		return true;
	}

	// a query results API based on a source with a filter method that is expected to be called only once.  All iterative methods are
	// implemented in terms of forEach that will call the filter only once and subsequently use the promised results.
	// will also copy the `total` property as well.
	function queryFromFilter(source) {
		var promisedResults, started, callbacks = [];
		// this is the main iterative function that will ensure we will only do a low level iteratation of the result set once.
		function forEach(callback, thisObj) {
			if (started) {
				// we have already iterated the query results, just hook into the existing promised results
				callback && promisedResults.then(function(results) {
					results.forEach(callback, thisObj);
				});
			} else {
				// first call, start the filter iterator, getting the results as a promise, so we can connect to that each subsequent time
				callback && callbacks.push(callback);
				if(!promisedResults){
					promisedResults = source.filter(function(value) {
						started = true;
						for(var i = 0, l = callbacks.length; i < l; i++){
							callbacks[i].call(thisObj, value);
						}
						return true;
					});
				}
			}
			return promisedResults;
		}

		return {
			total: source.total,
			filter: function(callback, thisObj) {
				var done;
				return forEach(function(value) {
					if (!done) {
						done = !callback.call(thisObj, value);
					}
				});
			},
			forEach: forEach,
			map: function(callback, thisObj) {
				var mapped = [];
				return forEach(function(value) {
					mapped.push(callback.call(thisObj, value));
				}).then(function() {
					return mapped;
				});
			},
			then: function(callback, errback) {
				return forEach().then(callback, errback);
			}
		};
	}

	var IDBKeyRange = window.IDBKeyRange || window.webkitIDBKeyRange;
	return declare(null, {
		// summary:
		//		This is a basic store for IndexedDB. It implements dojo/store/api/Store.

		constructor: function(options) {
			// summary:
			//		This is a basic store for IndexedDB.
			// options:
			//		This provides any configuration information that will be mixed into the store

			declare.safeMixin(this, options);
			var store = this;
			var dbConfig = this.dbConfig;
			this.indices = dbConfig.stores[this.storeName];
			this.cachedCount = {};
			for (var index in store.indices) {
				var value = store.indices[index];
				if (typeof value === 'number') {
					store.indices[index] = {
						preference: value
					};
				}
			}
			this.db = this.db || dbConfig.db;

			if (!this.db) {
				var openRequest = dbConfig.openRequest;
				if (!openRequest) {
					openRequest = dbConfig.openRequest = window.indexedDB.open(dbConfig.name || 'dojo-db',
						parseInt(dbConfig.version, 10));
					openRequest.onupgradeneeded = function() {
						var db = store.db = openRequest.result;
						for (var storeName in dbConfig.stores) {
							var storeConfig = dbConfig.stores[storeName];
							if (!db.objectStoreNames.contains(storeName)) {
								var idProperty = storeConfig.idProperty || 'id';
								var idbStore = db.createObjectStore(storeName, {
									keyPath: idProperty,
									autoIncrement: storeConfig[idProperty] &&
										storeConfig[idProperty].autoIncrement || false
								});
							} else {
								idbStore = openRequest.transaction.objectStore(storeName);
							}
							for (var index in storeConfig) {
								if (!idbStore.indexNames.contains(index) && index !== 'autoIncrement' &&
										storeConfig[index].indexed !== false) {
									idbStore.createIndex(index, index, storeConfig[index]);
								}
							}
						}
					};
					dbConfig.available = makePromise(openRequest);
				}
				this.available = dbConfig.available.then(function(db){
					return store.db = db;
				});
			}
		},

		// idProperty: String
		//		Indicates the property to use as the identity property. The values of this
		//		property should be unique.
		idProperty: 'id',

		storeName: '',

		// indices:
		//		a hash of the preference of indices, indices that are likely to have very
		//		unique values should have the highest numbers
		//		as a reference, sorting is always set at 1, so properties that are higher than
		//		one will trigger filtering with index and then sort the whole set.
		//		we recommend setting boolean values at 0.1.
		indices: {
			/*
			property: {
				preference: 1,
				multiEntry: true
			}
			*/
		},

		queryEngine: SimpleQueryEngine,

		transaction: function() {
			var store = this;
			this._currentTransaction = null;// get rid of the last transaction
			return {
				abort: function() {
					store._currentTransaction.abort();
				},
				commit: function() {
					// noop, idb does auto-commits
					store._currentTransaction = null;// get rid of the last transaction
				}
			}
		},

		_getTransaction: function() {
			if (!this._currentTransaction) {
				this._currentTransaction = this.db.transaction([this.storeName], 'readwrite');
				var store = this;
				this._currentTransaction.oncomplete = function() {
					// null it out so we will use a new one next time
					store._currentTransaction = null;
				};
				this._currentTransaction.onerror = function(error) {
					console.error(error);
				};
			}
			return this._currentTransaction;
		},

		_callOnStore: function(method, args, index, returnRequest) {
			// calls a method on the IndexedDB store
			var store = this;
			return when(this.available, function callOnStore() {
				var currentTransaction = store._currentTransaction;
				if (currentTransaction) {
					var allowRetry = true;
				} else {
					currentTransaction = store._getTransaction();
				}
				var request, idbStore;
				if (allowRetry) {
					try {
						idbStore = currentTransaction.objectStore(store.storeName);
						if (index) {
							idbStore = idbStore.index(index);
						}
						request = idbStore[method].apply(idbStore, args);
					} catch(e) {
						if (e.name === 'TransactionInactiveError' || e.name === 'InvalidStateError') {
							store._currentTransaction = null;
							//retry
							return callOnStore();
						} else {
							throw e;
						}
					}
				} else {
					idbStore = currentTransaction.objectStore(store.storeName);
					if (index) {
						idbStore = idbStore.index(index);
					}
					request = idbStore[method].apply(idbStore, args);
				}
				return returnRequest ? request : makePromise(request);
			});
		},

		get: function(id) {
			// summary:
			//		Retrieves an object by its identity.
			// id: Number
			//		The identity to use to lookup the object
			// options: Object?
			// returns: dojo//Deferred

			return this._callOnStore('get',[id]);
		},

		getIdentity: function(object) {
			// summary:
			//		Returns an object's identity
			// object: Object
			//		The object to get the identity from
			// returns: Number

			return object[this.idProperty];
		},

		put: function(object, options) {
			// summary:
			//		Stores an object.
			// object: Object
			//		The object to store.
			// options: __PutDirectives?
			//		Additional metadata for storing the data.  Includes an "id"
			//		property if a specific id is to be used.
			// returns: dojo/Deferred

			options = options || {};
			this.cachedCount = {}; // clear the count cache
			return this._callOnStore(options.overwrite === false ? 'add' : 'put',[object]);
		},

		add: function(object, options) {
			// summary:
			//		Adds an object.
			// object: Object
			//		The object to store.
			// options: __PutDirectives?
			//		Additional metadata for storing the data.  Includes an "id"
			//		property if a specific id is to be used.
			// returns: dojo/Deferred

			options = options || {};
			options.overwrite = false;
			return this.put(object, options);
		},

		remove: function(id) {
			// summary:
			//		Deletes an object by its identity.
			// id: Number
			//		The identity to use to delete the object
			// returns: dojo/Deferred

			this.cachedCount = {}; // clear the count cache
			return this._callOnStore('delete', [id]);
		},

		query: function(query, options) {
			// summary:
			//		Queries the store for objects.
			// query: Object
			//		The query to use for retrieving objects from the store.
			// options: __QueryOptions?
			//		The optional arguments to apply to the resultset.
			// returns: dojo/store/api/Store.QueryResults
			//		The results of the query, extended with iterative methods.

			options = options || {};
			var start = options.start || 0;
			var count = options.count || Infinity;
			var sortOption = options.sort;
			var store = this;

			// an array, do a union
			if (query.forEach) {
				var sortOptions = {sort: sortOption};
				var sorter = this.queryEngine({}, sortOptions);
				var totals = [];
				var collectedCount = 0;
				var inCount = 0;
				return queryFromFilter({
					total: {
						then: function() {
							// do it lazily again
							return all(totals).then(function(totals) {
								return totals.reduce(function(a, b) {
									return a + b;
								}) * collectedCount / (inCount || 1);
							}).then.apply(this, arguments);
						}
					},
					filter: function(callback, thisObj) {
						var index = 0;
						var queues = [];
						var done;
						var collected = {};
						var results = [];
						// wait for all the union segments to complete
						return all(query.map(function(part, i) {
							var queue = queues[i] = [];
							function addToQueue(object) {
								// to the queue that is kept for each individual query for merge sorting
								queue.push(object);
								var nextInQueues = []; // so we can index of the selected choice
								var toMerge = [];
								while(queues.every(function(queue) {
										if (queue.length > 0) {
											var next = queue[0];
											if (next) {
												toMerge.push(next);
											}
											return nextInQueues.push(next);
										}
									})){
									if (index >= start + count || toMerge.length === 0) {
										done = true;
										return; // exit filter loop
									}
									var nextSelected = sorter(toMerge)[0];
									// shift it off the selected queue
									queues[nextInQueues.indexOf(nextSelected)].shift();
									if (index++ >= start) {
										results.push(nextSelected);
										if (!callback.call(thisObj, nextSelected)) {
											done = true;
											return;
										}
									}
									nextInQueues = [];// reset
									toMerge = [];
								}
								return true;

							}
							var queryResults = store.query(part, sortOptions);
							totals[i] = queryResults.total;
							return queryResults.filter(function(object) {
								if (done) {
									return;
								}
								var id = store.getIdentity(object);
								inCount++;
								if (id in collected) {
									return true;
								}
								collectedCount++;
								collected[id] = true;
								return addToQueue(object);
							}).then(function(results) {
								// null signifies the end of this particular query result
								addToQueue(null);
								return results;
							});
						})).then(function() {
							return results;
						});
					}
				});
			}

			var keyRange;
			var alreadySearchedProperty;
			var queryId = JSON.stringify(query) + '-' + JSON.stringify(options.sort);
			var advance;
			var bestIndex, bestIndexQuality = 0;
			var indexTries = 0;
			var filterValue;

			function tryIndex(indexName, quality, factor) {
				indexTries++;
				var indexDefinition = store.indices[indexName];
				if (indexDefinition && indexDefinition.indexed !== false) {
					quality = quality || indexDefinition.preference * (factor || 1) || 0.001;
					if (quality > bestIndexQuality) {
						bestIndexQuality = quality;
						bestIndex = indexName;
						return true;
					}
				}
				indexTries++;
			}

			for (var i in query) {
				// test all the filters as possible indices to drive the query
				filterValue = query[i];
				var range = false;
				var wildcard, newFilterValue = null;

				if (typeof filterValue === 'boolean') {
					// can't use booleans as filter keys
					continue;
				}

				if (filterValue) {
					if (filterValue.from || filterValue.to) {
						range = true;
						(function(from, to) {
							// convert a to/from object to a testable object with a keyrange
							newFilterValue = {
								test: function(value) {
									return !from || from <= value &&
											(!to || to >= value);
								},
								keyRange: from ?
										  to ?
										  IDBKeyRange.bound(from, to, filterValue.excludeFrom, filterValue.excludeTo) :
										  IDBKeyRange.lowerBound(from, filterValue.excludeFrom) :
										  IDBKeyRange.upperBound(to, filterValue.excludeTo)
							};
						})(filterValue.from, filterValue.to);
					} else if (typeof filterValue === 'object' && filterValue.contains) {
						// contains is for matching any value in a given array to any value in the target indices array
						// this expects a multiEntry: true index
						(function(contains) {
							var keyRange, first = contains[0];

							var wildcard = first && first.match && first.match(wildcardRe);
							if (wildcard) {
								first = wildcard[1];
								keyRange = IDBKeyRange.bound(first, first + '~');
							} else {
								keyRange = IDBKeyRange.only(first);
							}
							newFilterValue = {
								test: function(value) {
									return contains.every(function(item) {
										var wildcard = item && item.match && item.match(wildcardRe);
										if (wildcard) {
											item = wildcard[1];
											return value && value.some(function(part) {
												return part.slice(0, item.length) === item;
											});
										}
										return value && value.indexOf(item) > -1;
									} );
								},
								keyRange: keyRange
							};
						})(filterValue.contains);
					} else if((wildcard = filterValue.match && filterValue.match(wildcardRe))) {
						// wildcard matching
						var matchStart = wildcard[1];
						newFilterValue = new RegExp('^' + matchStart);
						newFilterValue.keyRange = IDBKeyRange.bound(matchStart, matchStart + '~');
					}
				}
				if (newFilterValue) {
					query[i] = newFilterValue;
				}
				tryIndex(i, null, range ? 0.1 : 1);
			}
			var descending;
			if (sortOption) {
				// this isn't necessarily the best heuristic to determine the best index
				var mainSort = sortOption[0];
				if (mainSort.attribute === bestIndex || tryIndex(mainSort.attribute, 1)) {
					descending = mainSort.descending;
				} else {
					// we need to sort afterwards now
					var postSorting = true;
					// we have to retrieve everything in this case
					start = 0;
					count = Infinity;
				}
			}
			var cursorRequestArgs;
			if (bestIndex) {
				if (bestIndex in query) {
					// we are filtering
					filterValue = query[bestIndex];
					if (filterValue && (filterValue.keyRange)) {
						keyRange = filterValue.keyRange;
					} else {
						keyRange = IDBKeyRange.only(filterValue);
					}
					alreadySearchedProperty = bestIndex;
				} else {
					keyRange = null;
				}
				cursorRequestArgs = [keyRange, descending ? 'prev' : 'next'];
			} else {
				// no index, no arguments required
				cursorRequestArgs = [];
			}
			// console.log("using index", bestIndex);
			var cachedPosition = store.cachedPosition;
			if (cachedPosition && cachedPosition.queryId === queryId &&
					cachedPosition.offset < start && indexTries > 1) {
				advance = cachedPosition.preFilterOffset + 1;
				// make a new copy, so we don't have concurrency issues
				store.cachedPosition = cachedPosition = lang.mixin({}, cachedPosition);
			} else {
				// cache of the position, tracking our traversal progress
				cachedPosition = store.cachedPosition = {
					offset: -1,
					preFilterOffset: -1,
					queryId: queryId
				};
				if (indexTries < 2) {
					// can skip to advance
					cachedPosition.offset = cachedPosition.preFilterOffset = (advance = start) - 1;
				}
			}
			var filter = this.queryEngine(query);
			// this is adjusted so we can compute the total more accurately
			var filteredResults = {
				total: {
					then: function(callback) {
						// make this a lazy promise, only executing if we need to
						var cachedCount = store.cachedCount[queryId];
						if (cachedCount){
							return callback(adjustTotal(cachedCount));
						} else {
							var countPromise = (keyRange ? store._callOnStore('count', [keyRange], bestIndex) : store._callOnStore('count'));
							return (this.then = countPromise.then(adjustTotal)).then.apply(this, arguments);
						}
						function adjustTotal(total) {
							// we estimate the total count base on the matching rate
							store.cachedCount[queryId] = total;
							return Math.round((cachedPosition.offset + 1.01) / (cachedPosition.preFilterOffset + 1.01) * total);
						}
					}
				},
				filter: function(callback, thisObj) {
					// this is main implementation of the the query results traversal, forEach and map use this method
					var deferred = new Deferred();
					var all = [];
					function openCursor() {
						// get the cursor
						when(store._callOnStore('openCursor', cursorRequestArgs, bestIndex, true), function(cursorRequest) {
							// this will be called for each iteration in the traversal
							cursorsRunning++;
							cursorRequest.onsuccess = function(event) {
								cursorsRunning--;
								var cursor = event.target.result;
								if (cursor) {
									if (advance) {
										// we can advance through and wait for the completion
										cursor.advance(advance);
										cursorsRunning++;
										advance = false;
										return;
									}
									cachedPosition.preFilterOffset++;
									try {
										var item = cursor.value;
										if (options.join) {
											item = options.join(item);
										}
										return when(item, function(item) {
											if (filter.matches(item)) {
												cachedPosition.offset++;
												if (cachedPosition.offset >= start) { // make sure we are after the start
													all.push(item);
													if (!callback.call(thisObj, item) || cachedPosition.offset >= start + count - 1) {
														// finished
														cursorRequest.lastCursor = cursor;
														deferred.resolve(all);
														queueCursor();
														return;
													}
												}
											}
											// submit our cursor to the priority queue for continuation, now or when our turn comes up
											return queueCursor(cursor, options.priority, function() {
												// retry function, that we provide to the queue to use if the cursor can't be continued due to interruption
												// if called, open the cursor again, and continue from our current position
												advance = cachedPosition.preFilterOffset;
												openCursor();
											});
										});
									} catch(e) {
										deferred.reject(e);
									}
								} else {
									deferred.resolve(all);
								}
								// let any other cursors start executing now
								queueCursor();
							};
							cursorRequest.onerror = function(error) {
								cursorsRunning--;
								deferred.reject(error);
								queueCursor();
							};
						});
					}
					openCursor();
					return deferred.promise;
				}
			};

			if (postSorting) {
				// we are using the index to do filtering, so we are going to have to sort the entire list
				var sorter = this.queryEngine({}, options);
				var sortedResults = lang.delegate(filteredResults.filter(yes).then(function(results) {
					return sorter(results);
				}));
				sortedResults.total = filteredResults.total;
				return new QueryResults(sortedResults);
			}
			return options.rawResults ? filteredResults : queryFromFilter(filteredResults);
		}
	});

});
