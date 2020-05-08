define([
"dojo/_base/declare",
"dojo/_base/event",
"dojo/_base/lang",
"dojo/on",
"dojo/dom-style",
"dojo/sniff",
"dijit/_WidgetBase",
"dojox/html/metrics"],

function(
	declare,
	event,
	lang,
	on,
	domStyle,
	has,
	_WidgetBase,
	metrics){

		return declare('dojox.calendar._ScrollBarBase', _WidgetBase, {

		// value: Number
		//		The value of the scroll bar in pixel offset.
		value: 0,

		// minimum: Number
		//		The minimum value of the scroll bar.
		minimum: 0,

		// maximum: Number
		//		The maximum value of the scroll bar.
		maximum: 100,

		// direction: String
		//		Direction of the scroll bar. Valid values are "vertical" or "horizontal".
		direction: "vertical",

		_vertical: true,

		_scrollHandle: null,

		containerSize: 0,

		buildRendering: function(){
			this.inherited(arguments);
			this.own(on(this.domNode, "scroll", lang.hitch(this, function(param) {
				this.value = this._getDomScrollerValue();
				this.onChange(this.value);
				this.onScroll(this.value);
			})));
		},

		_getDomScrollerValue : function() {
			if(this._vertical){
				return this.domNode.scrollTop;
			}

			var rtl = !this.isLeftToRight();
			if(rtl){
				if(has("webkit") || has("ie") == 7){
					if(this._scW == undefined){
						this._scW = metrics.getScrollbar().w;
					}
					return this.maximum - this.domNode.scrollLeft - this.containerSize + this._scW;
				}
				if(has("mozilla")){
					return -this.domNode.scrollLeft;
				}
				// ie>7 and others...
			}
			return this.domNode.scrollLeft;
		},

		_setDomScrollerValue : function(value) {
			this.domNode[this._vertical?"scrollTop":"scrollLeft"] = value;
		},

		_setValueAttr: function(value){
			value = Math.min(this.maximum, value);
			value = Math.max(this.minimum, value);
			if (this.value != value) {
				this.value = value;
				this.onChange(value);
				this._setDomScrollerValue(value);
			}
		},

		onChange: function(value){
			// summary:
			//		 An extension point invoked when the value has changed.
			// value: Integer
			//		The position of the scroll bar in pixels.
			// tags:
			//		callback
		},

		onScroll: function(value){
			// summary:
			//		 An extension point invoked when the user scrolls with the mouse.
			// value: Integer
			//		The position of the scroll bar in pixels.
			// tags:
			//		callback
		},

		_setMinimumAttr: function(value){
			value = Math.min(value, this.maximum);
			this.minimum = value;
		},

		_setMaximumAttr: function(value){
			value = Math.max(value, this.minimum);
			this.maximum = value;
			domStyle.set(this.content, this._vertical?"height":"width", value + "px");
		},

		_setDirectionAttr: function(value){
			if(value == "vertical"){
				value = "vertical";
				this._vertical = true;
			}else{
				value = "horizontal";
				this._vertical = false;
			}
			this._set("direction", value);
		}

	});

});
