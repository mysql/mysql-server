define("dojox/charting/bidi/action2d/Tooltip", ["dojo/_base/declare", "dojo/dom-style"],
	function(declare, domStyle){
	// module:
	//		dojox/charting/bidi/action2d/Tooltip		
	return declare(null, {
		_recheckPosition: function(obj,rect,position){
			if(!this.chart.isRightToLeft()){
				return;
			}
			var shift = this.chart.offsets.l - this.chart.offsets.r;
			if(obj.element == "marker"){
				rect.x = this.chart.dim.width - obj.cx + shift;
				position[0] = "before-centered";
				position[1] = "after-centered";
			}
			else if(obj.element == "circle"){
				rect.x = this.chart.dim.width - obj.cx - obj.cr + shift;
			}
			else if(obj.element == "bar" || obj.element == "column"){
				rect.x = this.chart.dim.width - rect.width - rect.x + shift;
				if(obj.element == "bar"){
					position[0] = "before-centered";
					position[1] = "after-centered";
				}
			}
			else if(obj.element == "candlestick"){
				rect.x = this.chart.dim.width + shift - obj.x;
			}
			else if(obj.element == "slice"){
				if((position[0] == "before-centered") || (position[0] == "after-centered")) {
					position.reverse();
				}
				rect.x = obj.cx + (obj.cx - rect.x);
			}
		},
		
		_format: function(tooltip){
			var isChartDirectionRtl = (domStyle.get(this.chart.node, "direction") == "rtl");
			var isBaseTextDirRtl = (this.chart.getTextDir(tooltip) == "rtl");
			if(isBaseTextDirRtl && !isChartDirectionRtl){
				return "<span dir = 'rtl'>" + tooltip +"</span>";
			}
			else if(!isBaseTextDirRtl && isChartDirectionRtl){
				return "<span dir = 'ltr'>" + tooltip +"</span>";
			}else{
				return tooltip;
			}
		}
	});
});

