define(["dojo/_base/declare", "dojo/_base/array", "./Columns", "./common"], 
	function(declare, array, Columns, dc){

	return declare("dojox.charting.plot2d.ClusteredColumns", Columns, {
		// summary:
		//		A plot representing grouped or clustered columns (vertical bars)
		getBarProperties: function(){
			var length = this.series.length;
			array.forEach(this.series, function(serie){if(serie.hidden){length--;}});
			var f = dc.calculateBarSize(this._hScaler.bounds.scale, this.opt, length);
			return {gap: f.gap, width: f.size, thickness: f.size, clusterSize: length};
		}
	});
});
