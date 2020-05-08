define([
	'intern!object',
	'intern/chai!assert',
	'../priority',
	'dojo/Deferred',
	'dojo/promise/all',
	'dojo/_base/declare',
	'dojo/store/Memory',
	'dojo/store/util/QueryResults'
], function (registerSuite, assert, priority, Deferred, all, declare, Memory, QueryResults) {

	var started = 0;
	function anAsyncMethod(query){
		return function(){
			started++;
			var results = this.inherited(arguments);
			var deferred = new Deferred();
			setTimeout(function(){
				deferred.resolve(results);
			}, 10);
			return query ? new QueryResults(deferred) : deferred;
		}
	}
	var AsyncMemory = declare(Memory, {
		get: anAsyncMethod(),
		put: anAsyncMethod(),
		add: anAsyncMethod(),
		query: anAsyncMethod(true)
	});
	var data = [
		{id: 1, name: 'one', prime: false, mappedTo: 'E', words: ['banana']},
		{id: 2, name: 'two', even: true, prime: true, mappedTo: 'D', words: ['banana', 'orange']},
		{id: 3, name: 'three', prime: true, mappedTo: 'C', words: ['apple', 'orange']},
		{id: 4, name: 'four', even: true, prime: false, mappedTo: null},
		{id: 5, name: 'five', prime: true, mappedTo: 'A'}
	];
	priorityStore = priority(new AsyncMemory({
		data: data
	}));

	registerSuite({
		name: "priority",
		order: function(){
			var results = [];
			var operations = [];
			var order = [];
			var initialData = data.slice(0);
			operations.push(priorityStore.query({}, {priority: 10}).forEach(function(object){
				// clear the data
				results.push(object);
			}).then(function(){
				order.push('query');
				assert.deepEqual(results, initialData);
			}));
			assert.strictEqual(started, 1);
			operations.push(priorityStore.get(1, {priority: 0}).then(function(object){
				order.push('get');
				assert.deepEqual(object, data[0]);
			}));
			assert.strictEqual(started, 1);
			operations.push(priorityStore.put({id: 6, name: 'six'}, {priority: 3}).then(function(object){
				order.push('put');
			}));
			assert.strictEqual(started, 2);
			operations.push(priorityStore.add({id: 7, name: 'seven'}, {priority: 1}).then(function(object){
				order.push('add');
			}));
			assert.strictEqual(started, 2);
			return all(operations).then(function(){
				assert.strictEqual(started, 5);
				assert.deepEqual(order, ['query', 'put', 'add', 'get']);
			})
		}
	});
});
