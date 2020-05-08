define([
	"dojo/_base/array",
	"dojo/_base/declare",
	"./_StoreMixin",
	"./ListItem",
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/_StoreListMixin"
], function(array, declare, StoreMixin, ListItem, has, BidiStoreListMixin){

	// module:
	//		dojox/mobile/_StoreListMixin

	var _StoreListMixin = declare(has("dojo-bidi") ? "dojox.mobile._NonBidiStoreListMixin" : "dojox.mobile._StoreListMixin", StoreMixin, {
		// summary:
		//		Mixin for widgets to generate the list items corresponding to
		//		the dojo/store data provider object.
		// description:
		//		Mixin for widgets to generate the list items corresponding to
		//		the dojo/store data provider object.
		//		By mixing this class into the widgets, the list item nodes are
		//		generated as the child nodes of the widget and automatically
		//		regenerated whenever the corresponding data items are modified.

		// append: Boolean
		//		If true, refresh() does not clear the existing items.
		append: false,

		// itemMap: Object
		//		An optional parameter mapping field names from the store to ItemList names.
		//		Example: itemMap:{text:'label', profile_image_url:'icon'}
		itemMap: null,

		// itemRenderer: ListItem class or subclass
		//		The class used to create list items. Default is dojox/mobile/ListItem.
		itemRenderer: ListItem,

		buildRendering: function(){
			this.inherited(arguments);
			if(!this.store){ return; }
			var store = this.store;
			this.store = null;
			this.setStore(store, this.query, this.queryOptions);
		},

		createListItem: function(/*Object*/item){
			// summary:
			//		Creates a list item widget.
			return new this.itemRenderer(this._createItemProperties(item));
		},
		
		_createItemProperties: function(/*Object*/item){
			// summary:
			//		Creates list item properties.
			var props = {};
			if(!item["label"]){
				props["label"] = item[this.labelProperty];
			}
			// TODO this code should be like for textDir in the bidi mixin createListItem method
			// however for that dynamic set/get of the dir property must be supported first
			// that is why for now as a workaround we keep the code here
			if(has("dojo-bidi") && typeof props["dir"] == "undefined"){
				props["dir"] = this.isLeftToRight() ? "ltr" : "rtl";
			}
			for(var name in item){
				props[(this.itemMap && this.itemMap[name]) || name] = item[name];
			}
			return props;
		},
		
		_setDirAttr: function(props){
			// summary:
			//		Set the 'dir' attribute to support Mirroring.
			//		To be implemented by the bidi/_StoreLisMixin.js
			return props;
		},
		generateList: function(/*Array*/items){
			// summary:
			//		Given the data, generates a list of items.
			if(!this.append){
				array.forEach(this.getChildren(), function(child){
					child.destroyRecursive();
				});
			}
			array.forEach(items, function(item, index){
				this.addChild(this.createListItem(item));
				if(item[this.childrenProperty]){
					array.forEach(item[this.childrenProperty], function(child, index){
						this.addChild(this.createListItem(child));
					}, this);
				}
			}, this);
		},

		onComplete: function(/*Array*/items){
			// summary:
			//		A handler that is called after the fetch completes.
			this.generateList(items);
		},

		onError: function(/*Object*/ /*===== errorData =====*/){
			// summary:
			//		An error handler.
		},

		onAdd: function(/*Object*/item, /*Number*/insertedInto){
			// summary:
			//		Calls createListItem and adds the new list item when a new data item has been added to the store.
			this.addChild(this.createListItem(item), insertedInto);
		},

		onUpdate: function(/*Object*/item, /*Number*/insertedInto){
			// summary:
			//		Updates an existing list item when a data item has been modified.
			this.getChildren()[insertedInto].set(this._createItemProperties(item));
		},

		onDelete: function(/*Object*/item, /*Number*/removedFrom){
			// summary:
			//		Deletes an existing item.
			this.getChildren()[removedFrom].destroyRecursive();
		}
	});
	return has("dojo-bidi") ? declare("dojox.mobile._StoreListMixin", [_StoreListMixin, BidiStoreListMixin]) : _StoreListMixin;	
});
