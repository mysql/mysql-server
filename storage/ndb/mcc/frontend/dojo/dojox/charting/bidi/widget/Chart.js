define(["dojo/_base/declare"],
	function(declare){
	// module:
	//		dojox/charting/bidi/widget/Chart						
	function validateTextDir(textDir){
		return /^(ltr|rtl|auto)$/.test(textDir) ? textDir : null;
	}
	
	return declare(null, {
		postMixInProperties: function(){
			// set initial textDir of the chart, if passed in the creation use that value
			// else use default value, following the GUI direction, this.chart doesn't exist yet
			// so can't use set("textDir", textDir). This passed to this.chart in it's future creation.
			this.textDir = this.params["textDir"] ? this.params["textDir"] : this.params["dir"];
		},
	
		_setTextDirAttr: function(/*String*/ textDir){
			if(validateTextDir(textDir) != null){
				this._set("textDir", textDir);
				this.chart.setTextDir(textDir);
			}
		},
		
		_setDirAttr: function(/*String*/ dir){
			this._set("dir", dir);
			this.chart.setDir(dir);			
		}
	});	
});
