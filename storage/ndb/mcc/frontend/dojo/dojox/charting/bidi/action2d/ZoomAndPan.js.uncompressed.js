define("dojox/charting/bidi/action2d/ZoomAndPan", ["dojo/_base/declare"],
	function(declare){
	// module:
	//		dojox/charting/bidi/action2d/ZoomAndPan	
	return declare(null, {
		_getDelta: function(event){
			var delta = this.inherited(arguments);
			return delta * (this.chart.isRightToLeft()? -1 : 1);				
		}
	});
});
