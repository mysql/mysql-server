define("dojox/charting/plot2d/StackedBars", ["dojo/_base/declare", "dojo/_base/lang", "./Bars", "./commonStacked"], 
	function(declare, lang, Bars, commonStacked){

	return declare("dojox.charting.plot2d.StackedBars", Bars, {
		// summary:
		//		The plot object representing a stacked bar chart (horizontal bars).
		getSeriesStats: function(){
			// summary:
			//		Calculate the min/max on all attached series in both directions.
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			var stats = commonStacked.collectStats(this.series, lang.hitch(this, "isNullValue")), t;
			stats.hmin -= 0.5;
			stats.hmax += 0.5;
			t = stats.hmin, stats.hmin = stats.vmin, stats.vmin = t;
			t = stats.hmax, stats.hmax = stats.vmax, stats.vmax = t;
			return stats; // Object
		},
		rearrangeValues: function(values, transform, baseline){
			return commonStacked.rearrangeValues.call(this, values, transform, baseline);
		}
	});
});
