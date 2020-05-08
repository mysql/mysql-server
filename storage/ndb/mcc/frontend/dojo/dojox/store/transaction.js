define(['dojo/store/Memory', 'dojo/store/Cache', 'dojo/when', 'dojo/aspect', 'dojo/_base/lang'],
	function(Memory, Cache, when, aspect, lang){
// summary:
//		This is a transaction managing store. When transaction is started, by calling store.transaction(),
//		all request operations (put, add, remove), are entered into a transaction log. When commit() is
//		called, the actions in the log are then committed. This store relies on three other stores,
//		extending the dojo/store/Cache:
//			* masterStore - like with Cache, this store is the ultimate authority, and is usually connected to the backend
//			* cachingStore - This store includes local data, including changes within the current transaction
//			* transactionLogStore - This store holds the log of actions that need to be committed to the masterStore on the next commit

	var defaultTransactionLogStore;
	var stores = {};
	var nextStoreId = 1;
	return function(options){
		options = options || {};
		var masterStore = options.masterStore;
		var cachingStore = options.cachingStore;
		var storeId = masterStore.id || masterStore.storeName || masterStore.name || (masterStore.id = nextStoreId++);
		if(storeId){
			stores[storeId] = masterStore;
		}
		var transactionLogStore = options.transactionLogStore || defaultTransactionLogStore ||
				(defaultTransactionLogStore = new Memory());
		var autoCommit = true;
		function addToLog(method){
			return function execute(target, options){
				var transactionStore = this;
				if(autoCommit){
					// immediately perform the action
					var result = masterStore[method](target, options);
					when(result, null, function(e){
						if(transactionStore.errorHandler(e)){
							// failed, and the errorHandler has signaled that it should be requeued
							autoCommit = false;
							options.error = e;
							execute.call(transactionStore, target, options);
							autoCommit = true;
						}
					});
					return result;
				}else{
					// add to the transaction log
					var previousId = method === 'remove' ? target : transactionStore.getIdentity(target);
					if(previousId !== undefined){
						var previous = cachingStore.get(previousId);
					}
					return when(previous, function(previous){
						return when(transactionLogStore.add({
							objectId: previousId,
							method: method,
							target: target,
							previous: previous,
							options: options,
							storeId: storeId
						}), function(){
							return target;
						});
					});
				}
			};
		}
		// we need to listen for any notifications from the master store, and propagate these to the caching store
		// this arguably should actually be done in dojo/store/Cache
		aspect.before(masterStore, 'notify', function(object, existingId){
			if(object){
				cachingStore.put(object);
			}else{
				cachingStore.remove(existingId);
			}
		});
		return new Cache(lang.delegate(masterStore, {
			put: addToLog('put'),
			add: addToLog('add'),
			remove: addToLog('remove'),
			errorHandler: function(error){
				// this is called whenever an error occurs when attempting to commit transactional actions
				// this can return true to indicate that the action should be reattempted in the next commit
				// this can return false to indicate that the action should be reverted
				// if this returns undefined, no action will be taken
				console.error(error);
				return true;
			},
			commit: function(){
				// commit everything in the transaction log
				autoCommit = true;
				var transactionStore = this;
				// query for everything in the log
				return transactionLogStore.query({}).map(function(action){
					//var options = action.options || {};
					var method = action.method;
					var store = stores[action.storeId];
					var target = action.target;
					var result;
					try{
						// execute the queued action
						result = store[method](target, action.options);
					}catch(e){
						result = transactionStore.errorHandler(e);
						if(result === true){
							// don't remove it from the log, let it be executed again
							return e;
						}else if(result === false){
							// revert, by sending out a notification and updating the caching store
							if(method === 'add'){
								cachingStore.remove(action.objectId);
							}else{
								cachingStore.put(target);
							}
							store.notify && store.notify(method === 'add' ? null : action.previous,
								method === 'remove' ? undefined : action.objectId);
						}
						result = e;
					}
					// fired it, can remove now
					// TODO: handle async
					transactionLogStore.remove(action.id);
					return result;
				});
			},
			transaction: function(){
				// start a new transaction (by just turning off autoCommit)
				autoCommit = false;
				var transactionStore = this;
				return {
					commit: function(){
						return transactionStore.commit();
					}
				};
			}
		}), cachingStore, options);
	};
});