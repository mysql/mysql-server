define(["dojo/_base/lang", "dojo/date", "dojo/cldr/supplemental","dojo/date/stamp"], function(lang, date, cldr, stamp) {

// summary: Advanced date manipulation utilities.

var time = {};


time.newDate = function(obj, dateClassObj){
	// summary:
	//		Creates a new Date object.
	// obj: Object
	//		This object can have several values:
	//		- the time in milliseconds since gregorian epoch.
	//		- a Date instance
	//		- a String instance that can be decoded by the dojo/date/stamp class.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.

	// returns: Date
	dateClassObj = dateClassObj || Date;  
	var d;
	
	if(typeof(obj) == "number"){
		return new dateClassObj(obj);
	}else if(obj.getTime){
		return new dateClassObj(obj.getTime());
	}else if(obj.toGregorian){
		d = obj.toGregorian();
		if(dateClassObj !== Date){
			d = new dateClassObj(d.getTime());
		}
		return d;
	}else if(typeof obj == "string"){
		d = stamp.fromISOString(obj);
		if(d === null){
			throw new Error("Cannot parse date string ("+obj+"), specify a \"decodeDate\" function that translates this string into a Date object"); // cannot build date
		}else if(dateClassObj !== Date){ // from Date to dateClassObj
			d = new dateClassObj(d.getTime());
		}
		return d;
	}

};

time.floorToDay = function(d, reuse, dateClassObj){
	// summary:
	//		Floors the specified date to the start of day.
	// date: Date
	//		The date to floor.
	// reuse: Boolean
	//		Whether use the specified instance or create a new one. Default is false.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.	
	// returns: Date
	dateClassObj = dateClassObj || Date;  
	
	if(!reuse){
		d = time.newDate(d, dateClassObj);
	}
	
	d.setHours(0, 0, 0, 0);
		
	return d;
};

time.floorToMonth = function(d, reuse, dateClassObj){
	// summary:
	//		Floors the specified date to the start of the date's month.
	// date: Date
	//		The date to floor.
	// reuse: Boolean
	//		Whether use the specified instance or create a new one. Default is false.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.	
	// returns: Date
	dateClassObj = dateClassObj || Date;  
	
	if(!reuse){
		d = time.newDate(d, dateClassObj);
	}
	
	d.setDate(1);
	d.setHours(0, 0, 0, 0);
	
	return d;
};


time.floorToWeek = function(d, dateClassObj, dateModule, firstDayOfWeek, locale){
	// summary:
	//		Floors the specified date to the beginning of week.
	// d: Date
	//		Date to floor.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.	
	// dateModule: Object?
	//		Object that contains the "add" method. By default dojo.date is used.
	// firstDayOfWeek: Integer?
	//		Optional day of week that overrides the one provided by the CLDR.	
	// locale: String?
	//		Optional locale used to determine first day of week.
	dateClassObj = dateClassObj || Date; 
	dateModule = dateModule || date;  	
	
	var fd = firstDayOfWeek == undefined || firstDayOfWeek < 0 ? cldr.getFirstDayOfWeek(locale) : firstDayOfWeek;
	var day = d.getDay();
	if(day == fd){
		return d;
	}
	return time.floorToDay(
		dateModule.add(d, "day", day > fd ? -day+fd : -day+fd-7),
		true, dateClassObj);
};

time.floor = function(date, unit, steps, reuse, dateClassObj){
	// summary:
	//		floors the date to the unit.
	// date: Date
	//		The date/time to floor.
	// unit: String
	//		The unit. Valid values are "minute", "hour", "day".
	// steps: Integer
	//		Valid for "minute" or "hour" units.
	// reuse: Boolean
	//		Whether use the specified instance or create a new one. Default is false.	
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.
	// returns: Date

	var d = time.floorToDay(date, reuse, dateClassObj);
	
	switch(unit){
		case "week":
			return time.floorToWeek(d, firstDayOfWeek, dateModule, locale);
		case "minute":
			d.setHours(date.getHours());
			d.setMinutes(Math.floor(date.getMinutes() /steps) * steps);
			break;
		case "hour":
			d.setHours(Math.floor(date.getHours() /steps) * steps);
			break;
	}
	return d;
};

time.isStartOfDay = function(d, dateClassObj, dateModule){
	// summary:
	//		Tests if the specified date represents the starts of day. 
	// d: Date
	//		The date to test.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.	
	// dateModule: Object?
	//		Object that contains the "add" method. By default dojo.date is used.
	// returns: Boolean
	dateModule = dateModule || date;
	return dateModule.compare(this.floorToDay(d, false, dateClassObj), d) == 0;
};

time.isToday = function(d, dateClassObj){
	// summary:
	//		Returns whether the specified date is in the current day.
	// d: Date
	//		The date to test.
	// dateClassObj: Object?
	//		The Date class used, by default the native Date.
	// returns: Boolean
	dateClassObj = dateClassObj || Date;
	var today = new dateClassObj();
	return d.getFullYear() == today.getFullYear() &&
				 d.getMonth() == today.getMonth() && 
				 d.getDate() == today.getDate();
};

time.isOverlapping = function(renderData, start1, end1, start2, end2, includeLimits){
	// summary:
	//		Computes if the first time range defined by the start1 and end1 parameters 
	//		is overlapping the second time range defined by the start2 and end2 parameters.
	// renderData: Object
	//		The render data.
	// start1: Date
	//		The start time of the first time range.
	// end1: Date
	//		The end time of the first time range.
	// start2: Date
	//		The start time of the second time range.
	// end2: Date
	//		The end time of the second time range.
	// includeLimits: Boolean
	//		Whether include the end time or not.
	// returns: Boolean
	if(start1 == null || start2 == null || end1 == null || end2 == null){
		return false;
	}
	
	var cal = renderData.dateModule;
	
	if(includeLimits){
		if(cal.compare(start1, end2) == 1 || cal.compare(start2, end1) == 1){
			return false;
		}					
	}else if(cal.compare(start1, end2) != -1 || cal.compare(start2, end1) != -1){
		return false;
	}
	return true; 
};

return time;
});