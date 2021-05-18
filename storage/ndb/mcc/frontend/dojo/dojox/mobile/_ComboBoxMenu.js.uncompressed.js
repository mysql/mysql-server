define("dojox/mobile/_ComboBoxMenu", [
	"dojo/_base/kernel",
	"dojo/_base/declare",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dijit/form/_ComboBoxMenuMixin",
	"dijit/_WidgetBase",
	"./_ListTouchMixin",
	"./scrollable",
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/_ComboBoxMenu"
],
	function(dojo, declare, domClass, domConstruct, ComboBoxMenuMixin, WidgetBase, ListTouchMixin, Scrollable, has, BidiComboBoxMenu){
	// module:
	//		dojox/mobile/_ComboBoxMenu

	var _ComboBoxMenu = declare(has("dojo-bidi") ? "dojox.mobile._NonBidiComboBoxMenu" : "dojox.mobile._ComboBoxMenu", [WidgetBase, ListTouchMixin, ComboBoxMenuMixin], {
		// summary:
		//		Focus-less menu for internal use in dojox/mobile/ComboBox.
		//		Abstract methods that must be defined externally:
		//
		//		- onChange: item was explicitly chosen (mousedown somewhere on the menu and mouseup somewhere on the menu);
		//		- onPage: next(1) or previous(-1) button pressed.
		// tags:
		//		private

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblComboBoxMenu",
		
		buildRendering: function(){
			this.domNode = this.focusNode = domConstruct.create("div", { "class":"mblReset" });
			this.containerNode = domConstruct.create("div", { style: { position:"absolute", top:0, left:0 } }, this.domNode); // needed for scrollable
			this.previousButton = domConstruct.create("div", { "class":"mblReset mblComboBoxMenuItem mblComboBoxMenuPreviousButton", role:"option" }, this.containerNode);
			this.nextButton = domConstruct.create("div", { "class":"mblReset mblComboBoxMenuItem mblComboBoxMenuNextButton", role:"option" }, this.containerNode);
			this.inherited(arguments);
		},

		_createMenuItem: function(){
			// override of the method from dijit/form/_ComboBoxMenu.
			return domConstruct.create("div", {
				"class": "mblReset mblComboBoxMenuItem" +(this.isLeftToRight() ? "" : " mblComboBoxMenuItemRtl"),
				role: "option"
			});
		},

		onSelect: function(/*DomNode*/ node){
			// summary:
			//		Add selected CSS.
			domClass.add(node, "mblComboBoxMenuItemSelected");
		},

		onDeselect: function(/*DomNode*/ node){
			// summary:
			//		Remove selected CSS.
			domClass.remove(node, "mblComboBoxMenuItemSelected");
		},

		onOpen: function(){
			// summary:
			//		Called when the menu opens.
			this.scrollable.init({
				domNode: this.domNode,
				containerNode: this.containerNode
			});
			this.scrollable.scrollTo({x:0, y:0});
		},

		onClose: function(){
			// summary:
			//		Called when the menu closes.
			this.scrollable.cleanup();
		},

		postCreate: function(){
			this.inherited(arguments);
			this.scrollable = new Scrollable();
			this.scrollable.resize = function(){}; // resize changes the height rudely
			// #18000
			var self = this;
			this.scrollable.isLeftToRight = function(){
				return self.isLeftToRight();
			};
		}
	});
	return has("dojo-bidi") ? declare("dojox.mobile._ComboBoxMenu", [_ComboBoxMenu, BidiComboBoxMenu]) : _ComboBoxMenu;
});
