// TODO: auto scroll?

define([
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/event",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-geometry",
	"dojo/dom-style",
	"dojo/touch",
	"dojo/dom-attr",
	"dijit/registry",
	"./ListItem",
	"./common"
], function(array, connect, declare, event, win, domClass, domGeometry, domStyle, touch, domAttr, registry, ListItem, common){

	// module:
	//		dojox/mobile/EditableRoundRectList

	return declare("dojox.mobile._EditableListMixin", null, {
		// summary:
		//		A rounded rectangle list.
		// description:
		//		EditableRoundRectList is a rounded rectangle list, which can be used to
		//		display a group of items. Each item must be	a dojox/mobile/ListItem.

		rightIconForEdit: "mblDomButtonGrayKnob",
		deleteIconForEdit: "mblDomButtonRedCircleMinus",

		// isEditing: Boolean
		//		A read-only flag that indicates whether the widget is in the editing mode.
		isEditing: false,

		destroy: function(){
			// summary:
			//		Destroys the widget.
			if(this._blankItem){
				this._blankItem.destroy();
			}
			this.inherited(arguments);
		},

		_setupMoveItem: function(/*DomNode*/node){
			// tags:
			//		private
			domStyle.set(node, {
				width: domGeometry.getContentBox(node).w + "px",
				top: node.offsetTop + "px"
			});
			domClass.add(node, "mblListItemFloat");
		},

		_resetMoveItem: function(/*DomNode*/node){
			// tags:
			//		private
			this.defer(function(){ // iPhone needs setTimeout (via defer)
				domClass.remove(node, "mblListItemFloat");
				domStyle.set(node, {
					width: "",
					top: ""
				});
			});
		},

		_onClick: function(e){
			// summary:
			//		Internal handler for click events.
			// tags:
			//		private
			if(e && e.type === "keydown" && e.keyCode !== 13){ return; }
			if(this.onClick(e) === false){ return; } // user's click action
			var item = registry.getEnclosingWidget(e.target);
			for(var n = e.target; n !== item.domNode; n = n.parentNode){
				if(n === item.deleteIconNode){
					connect.publish("/dojox/mobile/deleteListItem", [item]);
					this.onDeleteItem(item); //callback
					break;
				}
			}
		},

		onClick: function(/*Event*/ /*===== e =====*/){
			// summary:
			//		User-defined function to handle clicks.
			// tags:
			//		callback
		},

		_onTouchStart: function(e){
			// tags:
			//		private
			if(this.getChildren().length <= 1){ return; }
			if(!this._blankItem){
				this._blankItem = new ListItem();
			}
			var item = this._movingItem = registry.getEnclosingWidget(e.target);
			this._startIndex = this.getIndexOfChild(item);
			var rightIconPressed = false;
			for(var n = e.target; n !== item.domNode; n = n.parentNode){
				if(n === item.rightIconNode){
					rightIconPressed = true;
					domAttr.set(item.rightIconNode, "aria-grabbed", "true");
					domAttr.set(this.domNode, "aria-dropeffect", "move");
					break;
				}
			}
			if(!rightIconPressed){ return; }
			var ref = item.getNextSibling();
			ref = ref ? ref.domNode : null;
			this.containerNode.insertBefore(this._blankItem.domNode, ref);
			this._setupMoveItem(item.domNode);
			this.containerNode.appendChild(item.domNode);

			if(!this._conn){
				this._conn = [
					this.connect(this.domNode, touch.move, "_onTouchMove"),
					this.connect(win.doc, touch.release, "_onTouchEnd")
				];
			}
			this._pos = [];
			array.forEach(this.getChildren(), function(c, index){
				this._pos.push(domGeometry.position(c.domNode, true).y);
			}, this);
			this.touchStartY = e.touches ? e.touches[0].pageY : e.pageY;
			this._startTop = domGeometry.getMarginBox(item.domNode).t;
			event.stop(e);
		},

		_onTouchMove: function(e){
			// tags:
			//		private
			var y = e.touches ? e.touches[0].pageY : e.pageY;
			var index = this._pos.length - 1;
			for(var i = 1; i < this._pos.length; i++){
				if(y < this._pos[i]){
					index = i - 1;
					break;
				}
			}
			var item = this.getChildren()[index];
			var blank = this._blankItem;
			if(item !== blank){
				var p = item.domNode.parentNode;
				if(item.getIndexInParent() < blank.getIndexInParent()){
					p.insertBefore(blank.domNode, item.domNode);
				}else{
					p.insertBefore(item.domNode, blank.domNode);
				}
			}
			this._movingItem.domNode.style.top = this._startTop + (y - this.touchStartY) + "px";
		},

		_onTouchEnd: function(e){
			// tags:
			//		private
			var startIndex = this._startIndex;
			var endIndex = this.getIndexOfChild(this._blankItem);
						
			var ref = this._blankItem.getNextSibling();
			ref = ref ? ref.domNode : null;
			if(ref === null){ 
				//If the item is droped at the end of the list the endIndex is wrong
				endIndex--; 
			} 
			this.containerNode.insertBefore(this._movingItem.domNode, ref);
			this.containerNode.removeChild(this._blankItem.domNode);
			this._resetMoveItem(this._movingItem.domNode);

			array.forEach(this._conn, connect.disconnect);
			this._conn = null;
			
			this.onMoveItem(this._movingItem, startIndex, endIndex); //callback
			
			domAttr.set(this._movingItem.rightIconNode, "aria-grabbed", "false");
			domAttr.remove(this.domNode, "aria-dropeffect");
		},

		startEdit: function(){
			// summary:
			//		Starts the editing.
			this.isEditing = true;
			domClass.add(this.domNode, "mblEditableRoundRectList");
			array.forEach(this.getChildren(), function(child){
				if(!child.deleteIconNode){
					child.set("rightIcon", this.rightIconForEdit);
					if(child.rightIconNode){
						domAttr.set(child.rightIconNode, "role", "button");
						domAttr.set(child.rightIconNode, "aria-grabbed", "false");
					}
					child.set("deleteIcon", this.deleteIconForEdit);
					child.deleteIconNode.tabIndex = child.tabIndex;
					if(child.deleteIconNode){
						domAttr.set(child.deleteIconNode, "role", "button");
					}
				}
				child.rightIconNode.style.display = "";
				child.deleteIconNode.style.display = "";
				common._setTouchAction(child.rightIconNode, "none");
			}, this);
			if(!this._handles){
				this._handles = [
					this.connect(this.domNode, touch.press, "_onTouchStart"),
					this.connect(this.domNode, "onclick", "_onClick"),
					this.connect(this.domNode, "onkeydown", "_onClick") // for desktop browsers
				];
			}
			
			this.onStartEdit(); // callback
		},

		endEdit: function(){
			// summary:
			//		Ends the editing.
			domClass.remove(this.domNode, "mblEditableRoundRectList");
			array.forEach(this.getChildren(), function(child){
				child.rightIconNode.style.display = "none";
				child.deleteIconNode.style.display = "none";
				common._setTouchAction(child.rightIconNode, "auto");
			});
			if(this._handles){
				array.forEach(this._handles, this.disconnect, this);
				this._handles = null;
			}
			this.isEditing = false;
			
			this.onEndEdit(); // callback
		},
		
		
		onDeleteItem: function(/*Widget*/item){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		This function is called when a user clicks the delete
			//		button.
			//		You have to provide that function or subscribe to /dojox/mobile/deleteListItem, 
			//		otherwise the delete button will have no-effect.
			
		},
		
		onMoveItem: function(/*Widget*/item, /*int*/from, /*int*/to){
			// summary:
			//		Stub function to connect to from your application.
		},

		onStartEdit: function(){
			// summary:
			//		Stub function to connect to from your application.
		},

		onEndEdit: function(){
			// summary:
			//		Stub function to connect to from your application.
		}


	});
});
