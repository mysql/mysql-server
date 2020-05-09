define(["dojo/_base/lang", "dojo/on", "dijit/registry", "dojo/date/stamp"],
function(lang, on, registry, stamp){
	var _onResults = []; // events on array
	var opener;

	return {
		init: function(){
			opener = this.opener;
			var onResult = on(this.selDate1, "click", lang.hitch(this, function(){
				this.datePicker2.set("value", date);
				this.opener.show(this.selDate1, ['below-centered','above-centered','after','before']);
			})); 
			_onResults.push(onResult);

			var onResult = on(this.save, "click", lang.hitch(this, function(){
				this.opener.hide(true);
				date = this.selDate1.value = this.datePicker2.get("value");
			})); 
			_onResults.push(onResult);

			var onResult = on(this.cancel, "click", lang.hitch(this, function(){
					this.opener.hide(false);
			})); 
			_onResults.push(onResult);
			
			// initialize the global Date variable as today
			date = stamp.toISOString(new Date(), {selector: "date"});
		},

		beforeActivate: function(){
			//console.log("date view beforeActivate()");
		},

		// view destroy
		destroy: function(){
			var onResult = _onResults.pop();
			while(onResult){
				onResult.remove();
				onResult = _onResults.pop();
			}
		}
	};	
});
