define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dijit/form/_SearchMixin",
	"dojox/mobile/TextBox",
	"dojo/dom-class",
	"dojo/keys",
	"dojo/touch",
	"dojo/on",
	"./sniff"
], function(declare, lang, win, SearchMixin, TextBox, domClass, keys, touch, on, has){

	return declare("dojox.mobile.SearchBox", [TextBox, SearchMixin], {
		// summary:
		//		A non-templated base class for INPUT type="search".

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblTextBox mblSearchBox",

		// type: String
		//		Corresponds to the type attribute of the HTML `<input>` element.
		//		The value is "search".
		type: "search",

		placeHolder: "",

		// incremental: Boolean
		//		Set true to search on every key or false to only search after 
		//		pressing ENTER or cancel.
		incremental: true,

		_setIncrementalAttr: function(val){
			// summary:
			//		Custom setter so the INPUT doesn't get the incremental attribute set.
			// tags:
			//		private
			this.incremental = val;
		},

		_onInput: function(e){
			// tags:
			//		private
			if(e.charOrCode == keys.ENTER){
				e.charOrCode = 229;
			}else if(!this.incremental){
				e.charOrCode = 0; // call _onInput to make sure a pending query is aborted
			}
			this.inherited(arguments);
		},

		postCreate: function(){
			this.inherited(arguments);
			this.textbox.removeAttribute('incremental'); // only want onsearch to fire for ENTER and cancel
			if(!this.textbox.hasAttribute('results')){
				this.textbox.setAttribute('results', '0'); // enables webkit search decoration
			}
			if(has("ios") < 5){
				domClass.add(this.domNode, 'iphone4'); // cannot click cancel button after focus so just remove it
				this.connect(this.textbox, "onfocus", // if value changes between start of onfocus to end, then it was a cancel
					function(){
						if(this.textbox.value !== ''){
							this.defer(
								function(){
									if(this.textbox.value === ''){
										this._onInput({ charOrCode: keys.ENTER }); // emulate onsearch
									}
								}
							);
						}
					}
				);
			}
			this.connect(this.textbox, "onsearch",
				function(){
					this._onInput({ charOrCode: keys.ENTER });
				}
			);
			
			// Clear action for the close button (iOS specific)
			var _this = this;
			var touchStartX, touchStartY;
			var handleRelease;
			if(has("ios")){
				this.on(touch.press, function(evt){
					var rect;
					touchStartX = evt.touches ? evt.touches[0].pageX : evt.pageX;
					touchStartY = evt.touches ? evt.touches[0].pageY : evt.pageY;
					// As the native searchbox on iOS, clear on release, not on start.
					handleRelease = on(win.doc, touch.release,
						function(evt){
							var rect, dx, dy;
							if(_this.get("value") != ""){
								dx = evt.pageX - touchStartX;
								dy = evt.pageY - touchStartY;
								// Mimic the behavior of native iOS searchbox: 
								// if location of release event close to the location of start event:
								if(Math.abs(dx) <= 4 && Math.abs(dy) <= 4){
									evt.preventDefault();
									_this.set("value", "");
									_this._onInput({ charOrCode: keys.ENTER });
								}
							}
							if(handleRelease){ // possibly already cancelled/cleared on touch.press
								handleRelease.remove();
								handleRelease = null;
							}
						}
					);
					rect = _this.domNode.getBoundingClientRect();
					// if touched in the right-most 30 pixels of the search box
					if(rect.right - (evt.touches ? evt.touches[0].pageX : evt.pageX) >= 30){
						// cancel
						if(handleRelease){
							handleRelease.remove();
							handleRelease = null;
						} 
					}
				});
			}
		}
	});
});
