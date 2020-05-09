define([
	"dojo/_base/Deferred",
	"dojo/_base/declare"
], function(Deferred, declare){

	// module:
	//		dojox/mobile/_StoreMixin

	return declare("dojox.mobile._StoreMixin", null, {
		// summary:
		//		Mixin for widgets to enable dojo/store data store.
		// description:
		//		By mixing this class into a widget, it can get data through a
		//		dojo/store data store. The widget must implement the following
		//		methods to handle the retrieved data:
		//
		//		- onComplete(/*Array*/items), onError(/*Object*/errorData),
		//		- onUpdate(/*Object*/item, /*Number*/insertedInto), and
		//		- onDelete(/*Object*/item, /*Number*/removedFrom).
	
		// store: Object
		//		Reference to data provider object used by this widget.
		store: null,

		// query: Object
		//		A query that can be passed to 'store' to initially filter the items.
		query: null,

		// queryOptions: Object
		//		An optional parameter for the query.
		queryOptions: null,

		// labelProperty: String
		//		A property name (a property in the dojo/store item) that specifies that item's label.
		labelProperty: "label",

		// childrenProperty: String
		//		A property name (a property in the dojo/store item) that specifies that item's children.
		childrenProperty: "children",

		setStore: function(/*dojo/store/api/Store*/store, /*String*/query, /*Object*/queryOptions){
			// summary:
			//		Sets the store to use with this widget.
			if(store === this.store){ return null; }
			if(store){
				store.getValue = function(item, property){
					return item[property];
				};
			}
			this.store = store;
			this._setQuery(query, queryOptions);
			return this.refresh();
		},

		setQuery: function(/*String*/query, /*Object*/queryOptions){
			this._setQuery(query, queryOptions);
			return this.refresh();
		},

		_setQuery: function(/*String*/query, /*Object*/queryOptions){
			// tags:
			//		private
			this.query = query;
			this.queryOptions = queryOptions || this.queryOptions;
		},

		refresh: function(){
			// summary:
			//		Fetches the data and generates the list items.
			if(!this.store){ return null; }
			var _this = this;
			var promise = this.store.query(this.query, this.queryOptions);
			Deferred.when(promise, function(results){
				if(results.items){
					results = results.items; // looks like dojo/data style items array
				}
				if(promise.observe){
					if(_this._observe_h){
						_this._observe_h.remove();
					}
					_this._observe_h = promise.observe(function(object, previousIndex, newIndex){
						if(previousIndex != -1){
							if(newIndex != previousIndex){
								// item removed or moved
								_this.onDelete(object, previousIndex);
								if(newIndex != -1){
									if (_this.onAdd) {
										 // new widget with onAdd method defined
										_this.onAdd(object, newIndex);
									} else {
										// TODO remove in 2.0
										// compatibility with 1.8: onAdd did not exist, add was handled by onUpdate
										_this.onUpdate(object, newIndex);
									}
								}
							}else{
								// item modified
								// if onAdd is not defined, we are "bug compatible" with 1.8 and we do nothing.
								// TODO remove test in 2.0
								if(_this.onAdd){
									_this.onUpdate(object, newIndex);
								}
							}
						}else if(newIndex != -1){
							// item added
							if(_this.onAdd){
								 // new widget with onAdd method defined
								_this.onAdd(object, newIndex);
							}else{
								// TODO remove in 2.0
								// compatibility with 1.8: onAdd did not exist, add was handled by onUpdate
								_this.onUpdate(object, newIndex);
							}
						}
					}, true); // we want to be notified of updates
				}
				_this.onComplete(results);
			}, function(error){
				_this.onError(error);
			});
			return promise;
		},

		destroy: function(){
			if(this._observe_h){
				this._observe_h = this._observe_h.remove();
			}
			this.inherited(arguments);
		}

/*=====
		// Subclass MUST implement the following methods.

		, onComplete: function(items){
			// summary:
			//		A handler that is called after the fetch completes.
		},

		onError: function(errorData){
			// summary:
			//		An error handler.
		},

		onUpdate: function(item, insertedInto){
			// summary:
			//		Called when an existing data item has been modified in the store.
			//		Note: for compatibility with previous versions where only onUpdate was present,
			//		if onAdd is not defined, onUpdate will be called instead.
		},

		onDelete: function(item, removedFrom){
			// summary:
			//		Called when a data item has been removed from the store.
		},
		
		// Subclass should implement the following methods.

		onAdd: function(item, insertedInto){
			// summary:
			//		Called when a new data item has been added to the store.
			//		Note: for compatibility with previous versions where this function did not exist,
			//		if onAdd is not defined, onUpdate will be called instead.
		}
=====*/
	});
});
