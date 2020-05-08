define([
	"dojo/_base/declare",
	"dojo/Deferred",
	"dojo/store/Memory",
	"dojo/store/util/QueryResults",
	"dojo/store/util/SimpleQueryEngine"
], function(declare, Deferred, Memory, QueryResults, SimpleQueryEngine){

	// module:
	//		dijit/tests/AsyncMemoryStore

	return declare(null, {
		// summary:
		//		Wrapper on dojo/store/Memory that makes functions async (for testing)

		queryEngine: SimpleQueryEngine,

		constructor: function(hash){
			this.store = new Memory(hash);
		},

		get: function(id){
			var d = new Deferred(), store = this.store;
			try{
				d.resolve(store.get(id));
			}catch(e){
				d.reject(e);
			}
			return d;
		},

		getIdentity: function(object){
			return this.store.getIdentity(object);
		},

		put: function(object, directives){
			return this.store.put(object, directives);
		},

		add: function(object, directives){
			return this.store.add(object, directives);
		},

		remove: function(id){
			return this.store.remove(id);

		},

		query: function(query, options){
			var d = new Deferred(), store = this.store;
			setTimeout(function(){
				try{
					d.resolve(store.query(query, options));
				}catch(e){
					d.reject(e);
				}
			}, 10);
			return QueryResults(d);
		}
	});
});
