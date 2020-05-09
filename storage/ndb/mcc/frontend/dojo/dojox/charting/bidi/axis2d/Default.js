define(["dojo/_base/declare", "dojo/dom-style"],
	function(declare, domStyle){
	// module:
	//		dojox/charting/bidi/axis2d/Default			
	return declare(null, {
		labelTooltip: function(elem, chart, label, truncatedLabel, font, elemType){
			// additional preprocessing of the labels, needed for rtl base text direction in LTR
			// GUI, or for ltr base text direction for RTL GUI.

			var isChartDirectionRtl = (domStyle.get(chart.node,"direction") == "rtl");
			var isBaseTextDirRtl = (chart.getTextDir(label) == "rtl");

			if(isBaseTextDirRtl && !isChartDirectionRtl){
				label = "<span dir='rtl'>" + label +"</span>";
			}
			if(!isBaseTextDirRtl && isChartDirectionRtl){
				label = "<span dir='ltr'>" + label +"</span>";
			}
			this.inherited(arguments);
		},
		
		_isRtl: function(){
			return this.chart.isRightToLeft();
		}
	});
});

