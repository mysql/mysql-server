define("dojox/mobile/LongListMixin", [ "dojo/_base/array",
         "dojo/_base/lang",
         "dojo/_base/declare",
         "dojo/sniff",
         "dojo/dom-construct",
         "dojo/dom-geometry",
         "dijit/registry",
         "./common",
         "./viewRegistry" ],
		function(array, lang, declare, has, domConstruct, domGeometry, registry, dm, viewRegistry){

	// module:
	//		dojox/mobile/LongListMixin
	// summary:
	//		A mixin that enhances performance of long lists contained in scrollable views.

	return declare("dojox.mobile.LongListMixin", null, {
		// summary:
		//		This mixin enhances performance of very long lists contained in scrollable views.
		// description:
		//		LongListMixin enhances a list contained in a ScrollableView
		//		so that only a subset of the list items are actually contained in the DOM
		//		at any given time. 
		//		The parent must be a ScrollableView or another scrollable component
		//		that inherits from the dojox.mobile.scrollable mixin, otherwise the mixin has
		//		no effect. Also, editable lists are not yet supported, so lazy scrolling is
		//		disabled if the list's 'editable' attribute is true.
		//		If this mixin is used, list items must be added, removed or reordered exclusively
		//		using the addChild and removeChild methods of the list. If the DOM is modified
		//		directly (for example using list.containerNode.appendChild(...)), the list
		//		will not behave correctly.
		
		// pageSize: int
		//		Items are loaded in the DOM by chunks of this size.
		pageSize: 20,
		
		// maxPages: int
		//		When this limit is reached, previous pages will be unloaded.
		maxPages: 5,
		
		// unloadPages: int
		//		Number of pages that will be unloaded when maxPages is reached.
		unloadPages: 1,
		
		startup : function(){
			if(this._started){ return; }
			
			this.inherited(arguments);

			if(!this.editable){

				this._sv = viewRegistry.getEnclosingScrollable(this.domNode);

				if(this._sv){

					// Get all children already added (e.g. through markup) and initialize _items
					this._items = this.getChildren();

					// remove all existing items from the old container node
					this._clearItems();

					this.containerNode = domConstruct.create("div", null, this.domNode);

					// listen to scrollTo and slideTo from the parent scrollable object

					this.connect(this._sv, "scrollTo", lang.hitch(this, this._loadItems), true);
					this.connect(this._sv, "slideTo", lang.hitch(this, this._loadItems), true);

					// The _topDiv and _bottomDiv elements are place holders for the items
					// that are not actually in the DOM at the top and bottom of the list.

					this._topDiv = domConstruct.create("div", null, this.domNode, "first");
					this._bottomDiv = domConstruct.create("div", null, this.domNode, "last");

					this._reloadItems();
				}
			}
		},
		
		_loadItems : function(toPos){
			// summary:	Adds and removes items to/from the DOM when the list is scrolled.
			
			var sv = this._sv; 			// ScrollableView
			var h = sv.getDim().d.h;
			if(h <= 0){ return; } 			// view is hidden

			var cury = -sv.getPos().y; // current y scroll position
			var posy = toPos ? -toPos.y : cury;

			// get minimum and maximum visible y positions:
			// we use the largest area including both the current and new position
			// so that all items will be visible during slideTo animations
			var visibleYMin = Math.min(cury, posy),
				visibleYMax = Math.max(cury, posy) + h;
			
			// add pages at top and bottom as required to fill the visible area
			while(this._loadedYMin > visibleYMin && this._addBefore()){ }
			while(this._loadedYMax < visibleYMax && this._addAfter()){ }
		},
		
		_reloadItems: function(){
			// summary:	Resets the internal state and reloads items according to the current scroll position.

			// remove all loaded items
			this._clearItems();
			
			// reset internal state
			this._loadedYMin = this._loadedYMax = 0;
			this._firstIndex = 0;
			this._lastIndex = -1;
			this._topDiv.style.height = "0px";
			
			this._loadItems();
		},
		
		_clearItems: function(){
			// summary: Removes all currently loaded items.
			var c = this.containerNode;
			array.forEach(registry.findWidgets(c), function(item){
				c.removeChild(item.domNode);
			});
		},
		
		_addBefore: function(){
			// summary:	Loads pages of items before the currently visible items to fill the visible area.
			
			var i, count;
			
			var oldBox = domGeometry.getMarginBox(this.containerNode);
			
			for(count = 0, i = this._firstIndex-1; count < this.pageSize && i >= 0; count++, i--){
				var item = this._items[i];
				domConstruct.place(item.domNode, this.containerNode, "first");
				if(!item._started){
					item.startup();
				}
				this._firstIndex = i;
			}
			
			var newBox = domGeometry.getMarginBox(this.containerNode);

			this._adjustTopDiv(oldBox, newBox);
			
			if(this._lastIndex - this._firstIndex >= this.maxPages*this.pageSize){
				var toRemove = this.unloadPages*this.pageSize;
				for(i = 0; i < toRemove; i++){
					this.containerNode.removeChild(this._items[this._lastIndex - i].domNode);
				}
				this._lastIndex -= toRemove;
				
				newBox = domGeometry.getMarginBox(this.containerNode);
			}

			this._adjustBottomDiv(newBox);
			
			return count == this.pageSize;
		},
		
		_addAfter: function(){
			// summary:	Loads pages of items after the currently visible items to fill the visible area.
			
			var i, count;
			
			var oldBox = null;
			
			for(count = 0, i = this._lastIndex+1; count < this.pageSize && i < this._items.length; count++, i++){
				var item = this._items[i];
				domConstruct.place(item.domNode, this.containerNode);
				if(!item._started){
					item.startup();
				}
				this._lastIndex = i;
			}
			if(this._lastIndex - this._firstIndex >= this.maxPages*this.pageSize){
				oldBox = domGeometry.getMarginBox(this.containerNode);
				var toRemove = this.unloadPages*this.pageSize;
				for(i = 0; i < toRemove; i++){
					this.containerNode.removeChild(this._items[this._firstIndex + i].domNode);
				}
				this._firstIndex += toRemove;
			}
			
			var newBox = domGeometry.getMarginBox(this.containerNode);

			if(oldBox){
				this._adjustTopDiv(oldBox, newBox);
			}
			this._adjustBottomDiv(newBox);

			return count == this.pageSize;
		},
		
		_adjustTopDiv: function(oldBox, newBox){
			// summary:	Adjusts the height of the top filler div after items have been added/removed.
			
			this._loadedYMin -= newBox.h - oldBox.h;
			this._topDiv.style.height = this._loadedYMin + "px";
		},
		
		_adjustBottomDiv: function(newBox){
			// summary:	Adjusts the height of the bottom filler div after items have been added/removed.
			
			// the total height is an estimate based on the average height of the already loaded items
			var h = this._lastIndex > 0 ? (this._loadedYMin + newBox.h) / this._lastIndex : 0;
			h *= this._items.length - 1 - this._lastIndex;
			this._bottomDiv.style.height = h + "px";
			this._loadedYMax = this._loadedYMin + newBox.h;
		},
		
		_childrenChanged : function(){
			// summary: Called by addChild/removeChild, updates the loaded items.
			
			// Whenever an item is added or removed, this may impact the loaded items,
			// so we have to clear all loaded items and recompute them. We cannot afford 
			// to do this on every add/remove, so we use a timer to batch these updates.
			// There would probably be a way to update the loaded items on the fly
			// in add/removeChild, but at the cost of much more code...
			if(!this._qs_timer){
				this._qs_timer = this.defer(function(){
					delete this._qs_timer;
					this._reloadItems();
				});
			}
		},

		resize: function(){
			// summary: Loads/unloads items to fit the new size
			this.inherited(arguments);
			if(this._items){
				this._loadItems();
			}
		},
		
		// The rest of the methods are overrides of _Container and _WidgetBase.
		// We must override them because children are not all added to the DOM tree
		// under the list node, only a subset of them will really be in the DOM,
		// but we still want the list to look as if all children were there.

		addChild : function(/* dijit._Widget */widget, /* int? */insertIndex){
			// summary: Overrides dijit._Container
			if(this._items){
				if( typeof insertIndex == "number"){
					this._items.splice(insertIndex, 0, widget);
				}else{
					this._items.push(widget);
				}
				this._childrenChanged();
			}else{
				this.inherited(arguments);
			}
		},

		removeChild : function(/* Widget|int */widget){
			// summary: Overrides dijit._Container
			if(this._items){
				this._items.splice(typeof widget == "number" ? widget : this._items.indexOf(widget), 1);
				this._childrenChanged();
			}else{
				this.inherited(arguments);
			}
		},

		getChildren : function(){
			// summary: Overrides dijit._WidgetBase
			if(this._items){
				return this._items.slice(0);
			}else{
				return this.inherited(arguments);
			}
		},

		_getSiblingOfChild : function(/* dijit._Widget */child, /* int */dir){
			// summary: Overrides dijit._Container

			if(this._items){
				var index = this._items.indexOf(child);
				if(index >= 0){
					index = dir > 0 ? index++ : index--;
				}
				return this._items[index];
			}else{
				return this.inherited(arguments);
			}
		},
		
		generateList: function(/*Array*/items){
			// summary:
			//		Overrides dojox.mobile._StoreListMixin when the list is a store list.
			
			if(this._items && !this.append){
				// _StoreListMixin calls destroyRecursive to delete existing items, not removeChild,
				// so we must remove all logical items (i.e. clear _items) before reloading the store.
				// And since the superclass destroys all children returned by getChildren(), and
				// this would actually return no children because _items is now empty, we must
				// destroy all children manually first.
				array.forEach(this.getChildren(), function(child){
					child.destroyRecursive();
				});
				this._items = [];
			}
			this.inherited(arguments);
		}
	});
});
