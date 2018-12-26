//>>built
define("dojox/widget/PlaceholderMenuItem", ["dojo", "dijit", "dojox", "dijit/Menu","dijit/MenuItem"], function(dojo, dijit, dojox){
dojo.experimental("dojox.widget.PlaceholderMenuItem");

dojo.declare("dojox.widget.PlaceholderMenuItem", dijit.MenuItem, {
	// summary:
	//		A menu item that can be used as a placeholder.  Set the label
	//		of this item to a unique key and you can then use it to add new
	//		items at that location.  This item is not displayed.
	
	_replaced: false,
	_replacedWith: null,
	_isPlaceholder: true,

	postCreate: function(){
		this.domNode.style.display = "none";
		this._replacedWith = [];
		if(!this.label){
			this.label = this.containerNode.innerHTML;
		}
		this.inherited(arguments);
	},
	
	replace: function(/*dijit.MenuItem[]*/ menuItems){
		// summary:
		//		replaces this menu item with the given menuItems.  The original
		//		menu item is not actually removed from the menu - so if you want
		//		it removed, you must do that explicitly.
		// returns:
		//		true if the replace happened, false if not
		if(this._replaced){ return false; }

		var index = this.getIndexInParent();
		if(index < 0){ return false; }

		var p = this.getParent();

		dojo.forEach(menuItems, function(item){
			p.addChild(item, index++);
		});
		this._replacedWith = menuItems;

		this._replaced = true;
		return true;
	},
	
	unReplace: function(/*Boolean?*/ destroy){
		// summary:
		//		Removes menu items added by calling replace().  It returns the
		//		array of items that were actually removed (in case you want to
		//		clean them up later)
		// destroy:
		//		Also call destroy on any removed items.
		// returns:
		//		The array of items that were actually removed
		
		if(!this._replaced){ return []; }

		var p = this.getParent();
		if(!p){ return []; }

		var r = this._replacedWith;
		dojo.forEach(this._replacedWith, function(item){
			p.removeChild(item);
			if(destroy){
				item.destroyRecursive();
			}
		});
		this._replacedWith = [];
		this._replaced = false;

		return r; // dijit.MenuItem[]
	}
});

// Se need to extend dijit.Menu so that we have a getPlaceholders function.
dojo.extend(dijit.Menu, {
	getPlaceholders: function(/*String?*/ label){
		// summary:
		//		Returns an array of placeholders with the given label.  There
		//		can be multiples.
		// label:
		//		Label to search for - if not specified, then all placeholders
		//		are returned
		// returns:
		//		An array of placeholders that match the given label
		var r = [];

		var children = this.getChildren();
		dojo.forEach(children, function(child){
			if(child._isPlaceholder && (!label || child.label == label)){
				r.push(child);
			}else if(child._started && child.popup && child.popup.getPlaceholders){
				r = r.concat(child.popup.getPlaceholders(label));
			}else if(!child._started && child.dropDownContainer){
				var node = dojo.query("[widgetId]", child.dropDownContainer)[0];
				var menu = dijit.byNode(node);
				if(menu.getPlaceholders){
					r = r.concat(menu.getPlaceholders(label));
				}
			}
		}, this);
		return r; // dojox.widget.PlaceholderMenuItem[]
	}
});

return dojox.widget.PlaceholderMenuItem;
});
