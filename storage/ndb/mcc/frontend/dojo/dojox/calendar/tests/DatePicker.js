define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dijit/Calendar"],

function(
	declare,
	lang,
	Calendar){
					
	return declare("demo.DatePicker", Calendar, {
		
		minDate: null,
		maxDate: null,
		
		getClassForDate: function(date, locale){
			if(this.minDate && this.maxDate){
				var cal = this.dateModule;
				if(cal.compare(date, this.minDate) >= 0 && cal.compare(date, this.maxDate) <= 0){
					return "Highlighted";
				}				
			}
			return null;
		}
		
	});
});
