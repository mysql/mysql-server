define([
	"dojo/_base/declare",
	"dojo/touch",
	"./sniff",
	"dijit/form/_ListBase"
], function(declare, touch, has, ListBase){

	return declare( "dojox.mobile._ListTouchMixin", ListBase, {
		// summary:
		//		Focus-less menu to handle touch events consistently.
		// description:
		//		Focus-less menu to handle touch events consistently. Abstract
		//		method that must be defined externally:
		//
		//		- onClick: item was chosen (mousedown somewhere on the menu and mouseup somewhere on the menu).

		postCreate: function(){
			this.inherited(arguments);

			// For some reason in IE click event is fired immediately after user scrolled combobox control and released
			// his/her finger. As a fix we replace click with tap event that is fired correctly.
			if(!( (has("ie") === 10 || (!has("ie") && has("trident") > 6)) && typeof(MSGesture) !== "undefined")){
				this._listConnect("click", "_onClick");
			}else{
				this._listConnect(touch.press, "_onPress");

				var self = this,
					tapGesture = new MSGesture(),
					target;

				this._onPress = function(e){
					tapGesture.target = self.domNode;
					tapGesture.addPointer(e.pointerId);
					target = e.target;
				};

				this.on("MSGestureTap", function(e){
					self._onClick(e, target);
				});
			}
		},

		_onClick: function(/*Event*/ evt, /*DomNode*/ target){
			this._setSelectedAttr(target);
			this.onClick(target);
		}
	});
});
