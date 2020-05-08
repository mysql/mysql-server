define(["dojo/_base/declare", "dojo/_base/lang", "./Columns", "./commonStacked"], 
	function(declare, lang, Columns, commonStacked){

	return declare("dojox.charting.plot2d.StackedColumns", Columns, {
		// summary:
		//		The plot object representing a stacked column chart (vertical bars).
		getSeriesStats: function(){
			// summary:
			//		Calculate the min/max on all attached series in both directions.
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			var stats = commonStacked.collectStats(this.series, lang.hitch(this, "isNullValue"));
			stats.hmin -= 0.5;
			stats.hmax += 0.5;
			return stats; // Object
		},

		rearrangeValues: function(values, transform, baseline){
			return commonStacked.rearrangeValues.call(this, values, transform, baseline);
 		}
	});
});
