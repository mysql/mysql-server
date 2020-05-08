define(["dojo/_base/declare", "dojo/_base/lang", "./Default", "./commonStacked"], 
	function(declare, lang, Default, commonStacked){

	return declare("dojox.charting.plot2d.Stacked", Default, {
		// summary:
		//		Like the default plot, Stacked sets up lines, areas and markers
		//		in a stacked fashion (values on the y axis added to each other)
		//		as opposed to a direct one.
		getSeriesStats: function(){
			// summary:
			//		Calculate the min/max on all attached series in both directions.
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			var stats = commonStacked.collectStats(this.series, lang.hitch(this, "isNullValue"));
			return stats; // Object
		},
		
		buildSegments: function(i, indexed){
			var run = this.series[i],
				min = indexed?Math.max(0, Math.floor(this._hScaler.bounds.from - 1)):0,
				max = indexed?Math.min(run.data.length-1, Math.ceil(this._hScaler.bounds.to)):run.data.length-1,
				rseg = null, segments = [], isNullValue = lang.hitch(this, "isNullValue");
			// split the run data into dense segments (each containing no nulls)
			// except if interpolates is false in which case ignore null between valid data
			for(var j = min; j <= max; j++){
				var value = indexed ? commonStacked.getIndexValue(this.series, i, j, isNullValue) : commonStacked.getValue(this.series, i, run.data[j] ?run.data[j].x: null, isNullValue);
				if(!isNullValue(value[0]) && (indexed || value[0].y != null)){
					if(!rseg){
						rseg = [];
						segments.push({index: j, rseg: rseg});
					}
					rseg.push(value[0]);
				}else{
					if(!this.opt.interpolate || indexed){
						// we break the line only if not interpolating or if we have indexed data
						rseg = null;
					}
				}
			}
			return segments;
		}
		
	});
});
