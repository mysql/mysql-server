define("dojox/charting/axis2d/Base", ["dojo/_base/declare", "../Element"],
	function(declare, Element){
	/*=====
	var __BaseAxisCtorArgs = {
		// summary:
		//		Optional arguments used in the definition of an invisible axis.
		// vertical: Boolean?
		//		A flag that says whether an axis is vertical (i.e. y axis) or horizontal. Default is false (horizontal).
		// min: Number?
		//		The smallest value on an axis. Default is 0.
		// max: Number?
		//		The largest value on an axis. Default is 1.
	};
	=====*/
	return declare("dojox.charting.axis2d.Base", Element, {
		// summary:
		//		The base class for any axis.  This is more of an interface/API
		//		definition than anything else; see dojox.charting.axis2d.Default
		//		for more details.
		constructor: function(chart, kwArgs){
			// summary:
			//		Return a new base axis.
			// chart: dojox/charting/Chart
			//		The chart this axis belongs to.
			// kwArgs: __BaseAxisCtorArgs?
			//		An optional arguments object to define the axis parameters.
			this.vertical = kwArgs && kwArgs.vertical;
			this.opt = {};
			this.opt.min = kwArgs && kwArgs.min;
			this.opt.max = kwArgs && kwArgs.max;
		},
		clear: function(){
			// summary:
			//		Stub function for clearing the axis.
			// returns: dojox/charting/axis2d/Base
			//		A reference to the axis for functional chaining.
			return this;	//	dojox/charting/axis2d/Base
		},
		initialized: function(){
			// summary:
			//		Return a flag as to whether or not this axis has been initialized.
			// returns: Boolean
			//		If the axis is initialized or not.
			return false;	//	Boolean
		},
		calculate: function(min, max, span){
			// summary:
			//		Stub function to run the calcuations needed for drawing this axis.
			// returns: dojox/charting/axis2d/Base
			//		A reference to the axis for functional chaining.
			return this;	//	dojox/charting/axis2d/Base
		},
		getScaler: function(){
			// summary:
			//		A stub function to return the scaler object created during calculate.
			// returns: Object
			//		The scaler object (see dojox.charting.scaler.linear for more information)
			return null;	//	Object
		},
		getTicks: function(){
			// summary:
			//		A stub function to return the object that helps define how ticks are rendered.
			// returns: Object
			//		The ticks object.
			return null;	//	Object
		},
		getOffsets: function(){
			// summary:
			//		A stub function to return any offsets needed for axis and series rendering.
			// returns: Object
			//		An object of the form { l, r, t, b }.
			return {l: 0, r: 0, t: 0, b: 0};	//	Object
		},
		render: function(dim, offsets){
			// summary:
			//		Stub function to render this axis.
			// returns: dojox/charting/axis2d/Base
			//		A reference to the axis for functional chaining.
			this.dirty = false;
			return this;	//	dojox/charting/axis2d/Base
		}
	});
});
