define("dojox/mobile/SpinWheel", [
	"dojo/_base/declare",
	"dojo/_base/array",
	"dojo/dom-construct",
	"./_PickerBase",
	"./SpinWheelSlot" // to load SpinWheelSlot for you (no direct references)
], function(declare, array, domConstruct, PickerBase){

	// module:
	//		dojox/mobile/SpinWheel

	return declare("dojox.mobile.SpinWheel", PickerBase, {
		// summary:
		//		A value picker widget that has spin wheels.
		// description:
		//		SpinWheel is a value picker component. It is a sectioned wheel
		//		that can be used to pick up some values from the wheel slots by
		//		spinning them.

		/* internal properties */	
		baseClass: "mblSpinWheel",

		buildRendering: function(){
			this.inherited(arguments);
			domConstruct.create("div", {className: "mblSpinWheelBar"}, this.domNode);
		},

		startup: function(){
			if(this._started){ return; }
			this.centerPos = Math.round(this.domNode.offsetHeight / 2);
			this.inherited(arguments);
		},

		resize: function() {
			this.centerPos = Math.round(this.domNode.offsetHeight / 2);
			array.forEach(this.getChildren(), function(child){
				child.resize && child.resize();
			});
		},

		addChild: function(/*Widget*/ widget, /*int?*/ insertIndex){
			this.inherited(arguments);
			if(this._started){
				widget.setInitialValue();
			}
		}
	});
});
