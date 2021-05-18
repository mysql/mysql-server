define("dojox/mobile/SimpleDialog", [
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-attr",
	"dojo/dom-construct",
	"dojo/on",
	"dojo/touch",
	"dijit/registry",
	"./Pane",
	"./iconUtils",
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/SimpleDialog"
], function(declare, win, domClass, domAttr, domConstruct, on, touch, registry, Pane, iconUtils, has, BidiSimpleDialog){
	// module:
	//		dojox/mobile/SimpleDialog

	var SimpleDialog = declare(has("dojo-bidi") ? "dojox.mobile.NonBidiSimpleDialog" : "dojox.mobile.SimpleDialog", Pane, {
		// summary:
		//		A dialog box for mobile.
		// description:
		//		SimpleDialog is a dialog box for mobile.
		//		When a SimpleDialog is created, it is initially hidden 
		//		(display="none"). To show the dialog box, you need to
		//		get a reference to the widget and to call its show() method.
		//
		//		The contents can be arbitrary HTML, text, or widgets. Note,
		//		however, that the widget is initially hidden. You need to be
		//		careful when you place in a SimpleDialog elements that cannot 
		//		be initialized in hidden state.
		//
		//		This widget has much less functionalities than dijit/Dialog, 
		//		but it has the advantage of a much smaller code size.

		// top: String
		//		The top edge position of the widget. If "auto", the widget is
		//		placed at the middle of the screen. Otherwise, the value
		//		(ex. "20px") is used as the top style of widget's domNode.
		top: "auto",

		// left: String
		//		The left edge position of the widget. If "auto", the widget is
		//		placed at the center of the screen. Otherwise, the value
		//		(ex. "20px") is used as the left style of widget's domNode.
		left: "auto",

		// modal: Boolean
		//		If true, a translucent cover is added over the entire page to
		//		prevent the user from interacting with elements on the page.
		modal: true,

		// closeButton: [const] Boolean
		//		If true, a button to close the dialog box is displayed at the
		//		top-right corner.
		//		Note that changing the value of the property after the widget
		//		creation has no effect.
		closeButton: false,

		// closeButtonClass: String
		//		A class name of a DOM button to be used as a close button.
		closeButtonClass: "mblDomButtonSilverCircleRedCross",

		// tabIndex: String
		//		Tabindex setting for the item so users can hit the tab key to
		//		focus on it.
		tabIndex: "0",
		
		// _setTabIndexAttr: [private] String
		//		Sets tabIndex to domNode.
		_setTabIndexAttr: "",

		/* internal properties */	
		
		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblSimpleDialog",
		
		// _cover: [private] Array
		//		Array for sharing the cover instances.
		_cover: [],

		buildRendering: function(){
			this.containerNode = domConstruct.create("div", {className:"mblSimpleDialogContainer"});
			if(this.srcNodeRef){
				// reparent
				for(var i = 0, len = this.srcNodeRef.childNodes.length; i < len; i++){
					this.containerNode.appendChild(this.srcNodeRef.removeChild(this.srcNodeRef.firstChild));
				}
			}
			this.inherited(arguments);
			domAttr.set(this.domNode, "role", "dialog");
			
			if(this.containerNode.getElementsByClassName){ //TODO: Do we need to support IE8 a11y?
	            var titleNode = this.containerNode.getElementsByClassName("mblSimpleDialogTitle")[0];
	            if (titleNode){
	            	titleNode.id = titleNode.id || registry.getUniqueId("dojo_mobile_mblSimpleDialogTitle");
	            	domAttr.set(this.domNode, "aria-labelledby", titleNode.id);
	            }
	            var textNode = this.containerNode.getElementsByClassName("mblSimpleDialogText")[0];
	            if (textNode){
	                textNode.id = textNode.id || registry.getUniqueId("dojo_mobile_mblSimpleDialogText");
	                domAttr.set(this.domNode, "aria-describedby", textNode.id);
	            }
			}
			domClass.add(this.domNode, "mblSimpleDialogDecoration");
			this.domNode.style.display = "none";
			this.domNode.appendChild(this.containerNode);
			if(this.closeButton){
				this.closeButtonNode = domConstruct.create("div", {
					className: "mblSimpleDialogCloseBtn "+this.closeButtonClass
				}, this.domNode);
				iconUtils.createDomButton(this.closeButtonNode);
				this.connect(this.closeButtonNode, "onclick", "_onCloseButtonClick");
			}
			this.connect(this.domNode, "onkeydown", "_onKeyDown"); // for desktop browsers
		},

		startup: function(){
			if(this._started){ return; }
			this.inherited(arguments);
			win.body().appendChild(this.domNode);
		},

		addCover: function(){
			// summary:
			//		Adds the transparent DIV cover.
			if(!this._cover[0]){
				this._cover[0] = domConstruct.create("div", {
					className: "mblSimpleDialogCover"
				}, win.body());
			}else{
				this._cover[0].style.display = "";
			}

			if(has("windows-theme")) {
				// Hack to prevent interaction with elements placed under cover div.
				this.own(on(this._cover[0], touch.press, function() {}));
			}
		},

		removeCover: function(){
			// summary:
			//		Removes the transparent DIV cover.
			this._cover[0].style.display = "none";
		},

		_onCloseButtonClick: function(e){
			// tags:
			//		private
			if(this.onCloseButtonClick(e) === false){ return; } // user's click action
			this.hide();
		},

		onCloseButtonClick: function(/*Event*/ /*===== e =====*/){
			// summary:
			//		User-defined function to handle clicks.
			// tags:
			//		callback
		},

		_onKeyDown: function(e){
			// tags:
			//		private
			if(e.keyCode == 27){ // ESC
				this.hide();
			}
		},

		refresh: function(){ // TODO: should we call refresh on resize?
			// summary:
			//		Refreshes the layout of the dialog.
			var n = this.domNode;
			var h;
			if(this.closeButton){
				var b = this.closeButtonNode;
				var s = Math.round(b.offsetHeight / 2);
				b.style.top = -s + "px";
				b.style.left = n.offsetWidth - s + "px";
			}
			if(this.top === "auto"){
				h = win.global.innerHeight || win.doc.documentElement.clientHeight;
				n.style.top = Math.round((h - n.offsetHeight) / 2) + "px";
			}else{
				n.style.top = this.top;
			}
			if(this.left === "auto"){
				h = win.global.innerWidth || win.doc.documentElement.clientWidth;
				n.style.left = Math.round((h - n.offsetWidth) / 2) + "px";
			}else{
				n.style.left = this.left;
			}
		},

		show: function(){
			// summary:
			//		Shows the dialog.
			if(this.domNode.style.display === ""){ return; }
			if(this.modal){
				this.addCover();
			}
			this.domNode.style.display = "";
			this.resize(); // #15628
			this.refresh();
			var diaglogButton;
			if(this.domNode.getElementsByClassName){
				diaglogButton = this.domNode.getElementsByClassName("mblSimpleDialogButton")[0];
			}
			var focusNode = diaglogButton || this.closeButtonNode || this.domNode; // Focus preference is: user supplied button, close button, entire dialog
			/// on Safari iOS the focus is not taken without a timeout
			this.defer(function(){ focusNode.focus();}, 1000);
		},

		hide: function(){
			// summary:
			//		Hides the dialog.
			if(this.domNode.style.display === "none"){ return; }
			this.domNode.style.display = "none";
			if(this.modal){
				this.removeCover();
			}
		}
	});
	return has("dojo-bidi") ? declare("dojox.mobile.SimpleDialog", [SimpleDialog, BidiSimpleDialog]) : SimpleDialog;
});
