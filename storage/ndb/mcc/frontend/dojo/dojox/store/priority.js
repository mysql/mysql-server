define(['dojo/_base/lang', 'dojo/Deferred', 'dojo/when'], function(lang, Deferred, when){
// summary:
//		This is a store wrapper that provides prioritized operations. One can include a "priority" property in the options
//		property that is a number representing the priority, for get, put, add, remove, or query calls. The associated
//		action will execute before any other actions in the queue that have a lower priority. The priority queue will also
//		throttle the execution of actions to limit the number of concurrent actions to the priority number. If you submit
//		an action with a priority of 3, it will not be executed until there are at most 2 other concurrent actions.

	var queue = []; // the main queue
	var running = 0; // how many actions are running
	function processQueue(){
		// here we pull actions of the queue and run them
		// each priority level has it's own sub-queue, we go
		// from highest to lowest, stopping if we hit the minimum
		// priority for the currently running operations
		for(var priority = queue.length - 1; priority >= running; priority--){
			// get the sub-queue for this priority
			var queueForPriorityLevel = queue[priority];
			var action = queueForPriorityLevel && queueForPriorityLevel[queueForPriorityLevel.length - 1];
			if(action){
				queueForPriorityLevel.pop();
				// track the number currently running
				running++;
				try{
					action.executor(function(){
						running--;
						// once finished, process the next one in the queue
						processQueue();
					});
				}catch(e){
					action.def.reject(e);
					processQueue();
				}
			}
		}
	}
	function deferredResults(){
		// because a query result set is not merely a promise that can be piped through, we have
		// to recreate the full result set API to defer the execution of query, within each of the
		// methods we wait for the promise for the query result to finish. We have to wrap this
		// in an object wrapper so that the returned promise doesn't further defer access to the
		// query result set
		var deferred = new Deferred();
		return  {
			promise: {
				total: {
					// allow for lazy access to the total
					then: function(callback, errback){
						return deferred.then(function(wrapper){
							return wrapper.results.total;
						}).then(callback, errback);
					}
				},
				forEach: function(callback, thisObj){
					// wait for the action to be executed
					return deferred.then(function(wrapper){
						// now run the forEach
						return wrapper.results.forEach(callback, thisObj);
					});
				},
				map: function(callback, thisObj){
					return deferred.then(function(wrapper){
						return wrapper.results.map(callback, thisObj);
					});
				},
				filter: function(callback, thisObj){
					return deferred.then(function(wrapper){
						return wrapper.results.filter(callback, thisObj);
					});
				},
				then: function(callback, errback){
					return deferred.then(function(wrapper){
						return when(wrapper.results, callback, errback);
					});
				}
			},
			resolve: deferred.resolve,
			reject: deferred.reject
		};
	}
	return function(store, config){
		config = config || {};
		var priorityStore = lang.delegate(store);
		// setup the handling for each of these methods
		['add', 'put', 'query', 'remove', 'get'].forEach(function(method){
			var original = store[method];
			if(original){
				priorityStore[method] = function(first, options){
					options = options || {};
					var def, executor;
					if(options.immediate){
						// if immediate is set, skip the queue
						return original.call(store, first, options);
					}
					// just in case a method calls another method with the same args,
					// default to doing the second call immediately.
					options.immediate = true;
					if(method === 'query'){
						// use the special query result deferral
						executor = function(callback){
							// execute and resolve the wrapper
							var queryResults = original.call(store, first, options);
							def.resolve({results: queryResults});
							// wait until the query results are done before performing the next action
							when(queryResults, callback, callback);
						};
						// the special query deferred
						def = deferredResults();
					}else{
						executor = function(callback){
							// execute the main action (for get, put, add, remove)
							when(original.call(store, first, options), function(value){
								def.resolve(value);
								callback(); // done
							},
							function(error){
								def.reject(error);
								callback();
							});

						};
						// can use a standard deferred
						def = new Deferred();
					}
					// add to the queue
					var priority = options.priority > -1 ?
						options.priority :
						config.priority > -1 ?
							config.priority : 4;
					(queue[priority] || (queue[priority] = [])).push(
						{executor:  executor, def: def});
					// get the next one off the queue
					processQueue();
					return def.promise;
				};
			}
		});
		return priorityStore;
	};
});