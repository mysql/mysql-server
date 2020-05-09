define([
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/dom",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/touch",
	"dojo/on",
	"./common",
	"dijit/_WidgetBase",
	"dijit/form/_ButtonMixin",
	"dijit/form/_FormWidgetMixin",
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/Button"
	],
	function(array, declare, win, dom, domClass, domConstruct, touch, on, common, WidgetBase, ButtonMixin, FormWidgetMixin, has, BidiButton){

	var Button = declare(has("dojo-bidi") ? "dojox.mobile.NonBidiButton" : "dojox.mobile.Button", [WidgetBase, FormWidgetMixin, ButtonMixin], {
		// summary:
		//		Non-templated BUTTON widget with a thin API wrapper for click 
		//		events and for setting the label.
		//
		//		Buttons can display a label, an icon, or both.
		//		A label should always be specified (through innerHTML) or the label
		//		attribute.  It can be hidden via showLabel=false.
		// example:
		//	|	<button data-dojo-type="dojox/mobile/Button" onClick="...">Hello world</button>

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblButton",

		// _setTypeAttr: [private] Function 
		//		Overrides the automatic assignment of type to nodes, because it causes
		//		exception on IE. Instead, the type must be specified as this.type
		//		when the node is created, as part of the original DOM.
		_setTypeAttr: null,

		/*=====
		// label: String
		//		The label of the button.
		label: "",
		=====*/
		

		isFocusable: function(){ 
			// Override of the method of dijit/_WidgetBase.
			return false; 
		},

		buildRendering: function(){
			if(!this.srcNodeRef){
				this.srcNodeRef = domConstruct.create("button", {"type": this.type});
			}else if(this._cv){
				var n = this.srcNodeRef.firstChild;
				if(n && n.nodeType === 3){
					n.nodeValue = this._cv(n.nodeValue);
				}
			}
			this.inherited(arguments);
			this.focusNode = this.domNode;
		},

		postCreate: function(){
			this.inherited(arguments);

			// we need to ensure the synthetic click is emitted by 
			// touch.doClicks even if we moved (inside or outside) before we 
			// released in the button area.
			this.domNode.dojoClick = "useTarget";
			// handle touch.press event
			var _this = this;
			this.on(touch.press, function(e){
				e.preventDefault();

				if(_this.domNode.disabled){return;}
				_this._press(true);

				// change button state depending on where we are
				var isFirstMoveDone = false;
				_this._moveh = on(win.doc, touch.move, function(e){
					if(!isFirstMoveDone){
						// #17220: preventDefault may not have any effect.
						// causing minor impact on some android 
						// (Galaxy Tab 2 with stock browser 4.1.1) where button
						// display does not reflect the actual button state 
						// when user moves back and forth from the button area.
						e.preventDefault();
						isFirstMoveDone = true;
					}
					_this._press(dom.isDescendant(e.target, _this.domNode));
				});

				// handle touch.release 
				_this._endh = on(win.doc, touch.release, function(e){
					_this._press(false);
					_this._moveh.remove();
					_this._endh.remove();
				});
			});

			common.setSelectable(this.focusNode, false);
			this.connect(this.domNode, "onclick", "_onClick");
		},

		_press: function(pressed){
			// tags:
			//		private
			if(pressed != this._pressed){
				this._pressed = pressed;
				var button = this.focusNode || this.domNode;
				var newStateClasses = (this.baseClass + ' ' + this["class"]).split(" ");
				newStateClasses = array.map(newStateClasses, function(c){
					return c + "Selected";
				});
				domClass.toggle(button, newStateClasses, pressed);
			}
		},

		_setLabelAttr: function(/*String*/ content){
			// tags:
			//		private
			this.inherited(arguments, [this._cv ? this._cv(content) : content]);
		}
	});

	return has("dojo-bidi") ? declare("dojox.mobile.Button", [Button, BidiButton]) : Button;
});
