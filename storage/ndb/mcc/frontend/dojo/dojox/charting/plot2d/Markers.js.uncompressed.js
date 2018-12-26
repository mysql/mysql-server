//>>built
define("dojox/charting/plot2d/Markers", ["dojo/_base/declare", "./Default"], function(declare, Default){
/*=====
var Default = dojox.charting.plot2d.Default
=====*/
	return declare("dojox.charting.plot2d.Markers", Default, {
		//	summary:
		//		A convenience plot to draw a line chart with markers.
		constructor: function(){
			//	summary:
			//		Set up the plot for lines and markers.
			this.opt.markers = true;
		}
	});
});
