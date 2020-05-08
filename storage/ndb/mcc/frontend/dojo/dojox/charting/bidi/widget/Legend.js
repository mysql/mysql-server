define(["dojo/_base/declare", "dojo/dom", "dijit/registry", "dojo/_base/connect", "dojo/_base/array", "dojo/query"],
	function(declare, dom, widgetManager, hub, arrayUtil, query){
	// module:
	//		dojox/charting/bidi/widget/Legend	
	function validateTextDir(textDir){
		return /^(ltr|rtl|auto)$/.test(textDir) ? textDir : null;
	}
	
	return declare(null, {
		postMixInProperties: function(){
			// summary:
			//		Connect the setter of textDir legend to setTextDir of the chart,
			//		so _setTextDirAttr of the legend will be called after setTextDir of the chart is called.
			// tags:
			//		private

			// find the chart that is the owner of this legend, use it's
			// textDir
			if(!this.chart){
				if(!this.chartRef){ return; }
				var chart = widgetManager.byId(this.chartRef);
				if(!chart){
					var node = dom.byId(this.chartRef);
					if(node){
						chart = widgetManager.byNode(node);
					}else{
						return;
					}
				}
				this.textDir = chart.chart.textDir;
				hub.connect(chart.chart, "setTextDir", this, "_setTextDirAttr");

			}else{
				this.textDir = this.chart.textDir;
				hub.connect(this.chart, "setTextDir", this, "_setTextDirAttr");

			}
		},

		_setTextDirAttr: function(/*String*/ textDir){
			// summary:
			//		Setter for textDir.
			// description:
			//		Users shouldn't call this function; they should be calling
			//		set('textDir', value)
			// tags:
			//		private

			// only if new textDir is different from the old one
			if(validateTextDir(textDir) != null){
				if(this.textDir != textDir){
					this._set("textDir", textDir);
					// get array of all the labels
					var legendLabels = query(".dojoxLegendText", this._tr);
						// for every label calculate it's new dir.
						arrayUtil.forEach(legendLabels, function(label){
							label.dir = this.getTextDir(label.innerHTML, label.dir);
					}, this);
				}
			}
		}
	});	
});
