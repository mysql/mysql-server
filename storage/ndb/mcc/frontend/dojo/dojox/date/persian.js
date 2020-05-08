define(["dojox/main", "dojo/_base/lang", "dojo/date", "./persian/Date"], function(dojox, lang, dd, IDate){

var dpersian = lang.getObject("date.persian", true, dojox);

// Utility methods to do arithmetic calculations with persian.Dates

	// added for compat to date
dpersian.getDaysInMonth = function(/*dojox/date/persian/Date*/month){
	return month.getDaysInPersianMonth(month.getMonth(), month.getFullYear());
};

//TODO: define persian.isLeapYear?  Or should it be invalid, since it has different meaning?

dpersian.compare = function(/*dojox/date/persian/Date*/date1, /*dojox/date/persian/Date*/date2, /*String?*/portion){
	// summary:
	//		Compare two persian date objects by date, time, or both.
	// description:
	//		Returns 0 if equal, positive if a > b, else negative.
	// date1: dojox/date/persian/Date
	// date2: dojox/date/persian/Date
	//		If not specified, the current persian.Date is used.
	// portion:
	//		A string indicating the "date" or "time" portion of a Date object.
	//		Compares both "date" and "time" by default.  One of the following:
	//		"date", "time", "datetime"

	if(date1 instanceof IDate){
		date1 = date1.toGregorian();
	}
	if(date2 instanceof IDate){
		date2 = date2.toGregorian();
	}
	
	return dd.compare.apply(null, arguments);
};

dpersian.add = function(/*dojox/date/persian/Date*/date, /*String*/interval, /*int*/amount){
	// summary:
	//		Add to a Date in intervals of different size, from milliseconds to years
	// date: dojox/date/persian/Date
	//		Date object to start with
	// interval:
	//		A string representing the interval.  One of the following:
	//		"year", "month", "day", "hour", "minute", "second",
	//		"millisecond", "week", "weekday"
	// amount:
	//		How much to add to the date.

	//	based on and similar to dojo.date.add

	var newPersianDate = new IDate(date);

	switch(interval){
		case "day":
			newPersianDate.setDate(date.getDate() + amount);
			break;
		case "weekday":
			var day = date.getDay();
			if(((day + amount) < 5) && ((day + amount) > 0)){
				 newPersianDate.setDate(date.getDate() + amount);
			}else{
				var adddays = 0, /*weekend */
					remdays = 0;
				if(day == 5){//friday
					day = 4;
					remdays = (amount > 0) ?  -1 : 1;
				}else if(day == 6){ //shabat
					day = 4;
					remdays = (amount > 0) ? -2 : 2;
				}
				var add = (amount > 0) ? (5 - day - 1) : -day
				var amountdif = amount - add;
				var div = parseInt(amountdif / 5);
				if(amountdif % 5 != 0){
					adddays = (amount > 0)  ? 2 : -2;
				}
				adddays = adddays + div * 7 + amountdif % 5 + add;
				newPersianDate.setDate(date.getDate() + adddays +  remdays);
			}
			break;
		case "year":
			newPersianDate.setFullYear(date.getFullYear() + amount);
			break;
		case "week":
			amount *= 7;
			newPersianDate.setDate(date.getDate() + amount);
			break;
		case "month":
			var month = date.getMonth();
			newPersianDate.setMonth(month + amount);
			break;
		case "hour":
			newPersianDate.setHours(date.getHours() + amount);
			break;
		case "minute":
			newPersianDate._addMinutes(amount);
			break;
		case "second":
			newPersianDate._addSeconds(amount);
			break;
		case "millisecond":
			newPersianDate._addMilliseconds(amount);
			break;
	}

	return newPersianDate; // dojox.date.persian.Date
};

dpersian.difference = function(/*dojox/date/persian/Date*/date1, /*dojox/date/persian/Date?*/date2, /*String?*/interval){
	// summary:
	//		date2 - date1
	// date1: dojox/date/persian/Date
	// date2: dojox/date/persian/Date
	//		If not specified, the current dojox.date.persian.Date is used.
	// interval:
	//		A string representing the interval.  One of the following:
	//		"year", "month", "day", "hour", "minute", "second",
	//		"millisecond",  "week", "weekday"
	//
	//		Defaults to "day".

	//	based on and similar to dojo.date.difference

	date2 = date2 || new IDate();
	interval = interval || "day";
	var yearDiff = date2.getFullYear() - date1.getFullYear();
	var delta = 1; // Integer return value
	switch(interval){
		case "weekday":
			var days = Math.round(dpersian.difference(date1, date2, "day"));
			var weeks = parseInt(dpersian.difference(date1, date2, "week"));
			var mod = days % 7;

			// Even number of weeks
			if(mod == 0){
				days = weeks*5;
			}else{
				// Weeks plus spare change (< 7 days)
				var adj = 0;
				var aDay = date1.getDay();
				var bDay = date2.getDay();
	
				weeks = parseInt(days/7);
				mod = days % 7;
				// Mark the date advanced by the number of
				// round weeks (may be zero)
				var dtMark = new IDate(date1);
				dtMark.setDate(dtMark.getDate()+(weeks*7));
				var dayMark = dtMark.getDay();
	
				// Spare change days -- 6 or less
				if(days > 0){
					switch(true){
						// Range starts on Fri
						case aDay == 5:
							adj = -1;
							break;
						// Range starts on Sat
						case aDay == 6:
							adj = 0;
							break;
						// Range ends on Fri
						case bDay == 5:
							adj = -1;
							break;
						// Range ends on Sat
						case bDay == 6:
							adj = -2;
							break;
						// Range contains weekend
						case (dayMark + mod) > 5:
							adj = -2;
					}
				}else if(days < 0){
					switch(true){
						// Range starts on Fri
						case aDay == 5:
							adj = 0;
							break;
						// Range starts on Sat
						case aDay == 6:
							adj = 1;
							break;
						// Range ends on Fri
						case bDay == 5:
							adj = 2;
							break;
						// Range ends on Sat
						case bDay == 6:
							adj = 1;
							break;
						// Range contains weekend
						case (dayMark + mod) < 0:
							adj = 2;
					}
				}
				days += adj;
				days -= (weeks*2);
			}
			delta = days;
			break;
		case "year":
			delta = yearDiff;
			break;
		case "month":
			var startdate =  (date2.toGregorian() > date1.toGregorian()) ? date2 : date1; // more
			var enddate = (date2.toGregorian() > date1.toGregorian()) ? date1 : date2;
			
			var month1 = startdate.getMonth();
			var month2 = enddate.getMonth();
			
			if (yearDiff == 0){
				delta = startdate.getMonth() - enddate.getMonth() ;
			}else{
				delta = 12-month2;
				delta +=  month1;
				var i = enddate.getFullYear()+1;
				var e = startdate.getFullYear();
				for (i;   i < e;  i++){
					delta += 12;
				}
			}
			if (date2.toGregorian() < date1.toGregorian()){
				delta = -delta;
			}
			break;
		case "week":
			// Truncate instead of rounding
			// Don't use Math.floor -- value may be negative
			delta = parseInt(dpersian.difference(date1, date2, "day")/7);
			break;
		case "day":
			delta /= 24;
			// fallthrough
		case "hour":
			delta /= 60;
			// fallthrough
		case "minute":
			delta /= 60;
			// fallthrough
		case "second":
			delta /= 1000;
			// fallthrough
		case "millisecond":
			delta *= date2.toGregorian().getTime()- date1.toGregorian().getTime();
	}

	// Round for fractional values and DST leaps
	return Math.round(delta); // Number (integer)
};
return dpersian;
});
