define("dojox/date/persian/Date", ["dojo/_base/lang", "dojo/_base/declare", "dojo/date"], function(lang, declare, dd){

	var IDate = declare("dojox.date.persian.Date", null, {
	// summary:
	//		The component defines the Persian (Hijri) Calendar Object
	// description:
	//		This module is similar to the Date() object provided by JavaScript
	// example:
	//	|	var date = new dojox.date.persian.Date();
	//	|	document.writeln(date.getFullYear()+'\'+date.getMonth()+'\'+date.getDate());


	_date: 0,
	_month: 0,
	_year: 0,
	_hours: 0,
	_minutes: 0,
	_seconds: 0,
	_milliseconds: 0,
	_day: 0,
	_GREGORIAN_EPOCH : 1721425.5,
	_PERSIAN_EPOCH : 1948320.5,
	daysInMonth:[31,31,31,31,31,31,30,30,30,30,30,29 ],
	constructor: function(){
		// summary:
		//		This is the constructor
		// description:
		//		This function initialize the date object values
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	var date2 = new dojox.date.persian.Date("12\2\1429");
		//	|	var date3 = new dojox.date.persian.Date(date2);
		//	|	var date4 = new dojox.date.persian.Date(1429,2,12);

		var len = arguments.length;
		if(!len){// use the current date value, added "" to the similarity to date
			this.fromGregorian(new Date());
		}else if(len == 1){
			var arg0 = arguments[0];
			if(typeof arg0 == "number"){ // this is time "valueof"
				arg0 = new Date(arg0);
			}

			if(arg0 instanceof Date){
				this.fromGregorian(arg0);
			}else if(arg0 == ""){
				// date should be invalid.  Dijit relies on this behavior.
				this._date = new Date(""); //TODO: should this be NaN?  _date is not a Date object
			}else{  // this is Persian.Date object
				this._year = arg0._year;
				this._month =  arg0._month;
				this._date = arg0._date;
				this._hours = arg0._hours;
				this._minutes = arg0._minutes;
				this._seconds = arg0._seconds;
				this._milliseconds = arg0._milliseconds;
			}
		}else if(len >=3){
			// YYYY MM DD arguments passed, month is from 0-12
			this._year += arguments[0];
			this._month += arguments[1];
			this._date += arguments[2];
			this._hours += arguments[3] || 0;
			this._minutes += arguments[4] || 0;
			this._seconds += arguments[5] || 0;
			this._milliseconds += arguments[6] || 0;
		}
	},

	getDate:function(){
		// summary:
		//		This function returns the date value (1 - 30)
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	document.writeln(date1.getDate);
		return this._date;
	},
	
	getMonth:function(){
		// summary:
		//		This function return the month value ( 0 - 11 )
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	document.writeln(date1.getMonth()+1);

		return this._month;
	},

	getFullYear:function(){
		// summary:
		//		This function return the Year value
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	document.writeln(date1.getFullYear());

		return this._year;
	},
		
	getDay:function(){
		// summary:
		//		This function return Week Day value ( 0 - 6 )
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	document.writeln(date1.getDay());

		return this.toGregorian().getDay();
	},
		
	getHours:function(){
		// summary:
		//		returns the Hour value
		return this._hours;
	},
	
	getMinutes:function(){
		// summary:
		//		returns the Minutes value
		return this._minutes;
	},

	getSeconds:function(){
		// summary:
		//		returns the seconds value
		return this._seconds;
	},

	getMilliseconds:function(){
		// summary:
		//		returns the Milliseconds value
		return this._milliseconds;
	},

	setDate: function(/*number*/date){
		// summary:
		//		This function sets the Date
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	date1.setDate(2);

		date = parseInt(date);

		if(date > 0 && date <= this.getDaysInPersianMonth(this._month, this._year)){
			this._date = date;
		}else{
			var mdays;
			if(date>0){
				for(mdays = this.getDaysInPersianMonth(this._month, this._year);
					date > mdays;
						date -= mdays,mdays =this.getDaysInPersianMonth(this._month, this._year)){
					this._month++;
					if(this._month >= 12){this._year++; this._month -= 12;}
				}

				this._date = date;
			}else{
				for(mdays = this.getDaysInPersianMonth((this._month-1)>=0 ?(this._month-1) :11 ,((this._month-1)>=0)? this._year: this._year-1);
						date <= 0;
							mdays = this.getDaysInPersianMonth((this._month-1)>=0 ? (this._month-1) :11,((this._month-1)>=0)? this._year: this._year-1)){
					this._month--;
					if(this._month < 0){this._year--; this._month += 12;}

					date+=mdays;
				}
				this._date = date;
			}
		}
		return this;
	},

	setFullYear:function(/*number*/year){
		// summary:
		//		This function set Year
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	date1.setYear(1429);

		this._year = +year;
	},

	setMonth: function(/*number*/month) {
		// summary:
		//		This function set Month
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	date1.setMonth(2);

		this._year += Math.floor(month / 12);
		if(month > 0){
			this._month = Math.floor(month % 12);
		}else{
			this._month = Math.floor(((month % 12) + 12) % 12);
		}
	},

	setHours:function(){
		// summary:
		//		set the Hours
		var hours_arg_no = arguments.length;
		var hours = 0;
		if(hours_arg_no >= 1){
			hours = parseInt(arguments[0]);
		}

		if(hours_arg_no >= 2){
			this._minutes = parseInt(arguments[1]);
		}

		if(hours_arg_no >= 3){
			this._seconds = parseInt(arguments[2]);
		}

		if(hours_arg_no == 4){
			this._milliseconds = parseInt(arguments[3]);
		}

		while(hours >= 24){
			this._date++;
			var mdays = this.getDaysInPersianMonth(this._month, this._year);
			if(this._date > mdays){
					this._month ++;
					if(this._month >= 12){this._year++; this._month -= 12;}
					this._date -= mdays;
			}
			hours -= 24;
		}
		this._hours = hours;
	},

	_addMinutes: function(/*Number*/minutes){
		minutes += this._minutes;
		this.setMinutes(minutes);
		this.setHours(this._hours + parseInt(minutes / 60));
		return this;
	},

	_addSeconds: function(/*Number*/seconds){
		seconds += this._seconds;
		this.setSeconds(seconds);
		this._addMinutes(parseInt(seconds / 60));
		return this;
	},

	_addMilliseconds: function(/*Number*/milliseconds){
		milliseconds += this._milliseconds;
		this.setMilliseconds(milliseconds);
		this._addSeconds(parseInt(milliseconds / 1000));
		return this;
	},

	setMinutes: function(/*Number*/minutes){
		// summary:
		//		sets the minutes (0-59) only.
		this._minutes = minutes % 60;
		return this;
	},

	setSeconds: function(/*Number*/seconds){
		// summary:
		//		sets the seconds (0-59) only.
		this._seconds = seconds % 60;
		return this;
	},

	setMilliseconds: function(/*Number*/milliseconds){
		this._milliseconds = milliseconds % 1000;
		return this;
	},
		
	toString:function(){
		// summary:
		//		This returns a string representation of the date in "DDDD MMMM DD YYYY HH:MM:SS" format
		// example:
		//	|	var date1 = new dojox.date.persian.Date();
		//	|	document.writeln(date1.toString());

		//FIXME: TZ/DST issues?
		if(isNaN(this._date)){
			return "Invalidate Date";
		}else{
			var x = new Date();
			x.setHours(this._hours);
			x.setMinutes(this._minutes);
			x.setSeconds(this._seconds);
			x.setMilliseconds(this._milliseconds);
			return this._month+" "+ this._date + " " + this._year + " " + x.toTimeString();
		}
	},
		
		
	toGregorian:function(){
		// summary:
		//		This returns the equevalent Grogorian date value in Date object
		// example:
		//	|	var datePersian = new dojox.date.persian.Date(1429,11,20);
		//	|	var dateGregorian = datePersian.toGregorian();

		var hYear = this._year;
		var date,j;
		j = this.persian_to_jd(this._year,this._month+1,this._date);
		date = this.jd_to_gregorian(j,this._month+1);
		weekday = this.jwday(j);
		var _21=new Date(date[0],date[1]-1,date[2],this._hours,this._minutes,this._seconds,this._milliseconds);
		return _21;
	},

	
	// ported from the Java class com.ibm.icu.util.PersianCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
	fromGregorian:function(/*Date*/gdate){
		// summary:
		//		This function returns the equivalent Persian Date value for the Gregorian Date
		// example:
		//	|	var datePersian = new dojox.date.persian.Date();
		//	|	var dateGregorian = new Date(2008,10,12);
		//	|	datePersian.fromGregorian(dateGregorian);

		var _23=new Date(gdate);
		var _24=_23.getFullYear(),_25=_23.getMonth(),_26=_23.getDate();
		var persian = this.calcGregorian(_24,_25,_26);
		this._date=persian[2];
		this._month=persian[1];
		this._year=persian[0];
		this._hours=_23.getHours();
		this._minutes=_23.getMinutes();
		this._seconds=_23.getSeconds();
		this._milliseconds=_23.getMilliseconds();
		this._day=_23.getDay();
		return this;
	},
//  calcGregorian  --  Perform calculation starting with a Gregorian date
	calcGregorian:function (year,month,day){
	var j, weekday;
		    //  Update Julian day
		j = this.gregorian_to_jd(year, month + 1, day) +(Math.floor(0 + 60 * (0 + 60 * 0) + 0.5) / 86400.0);
		    //  Update Persian Calendar
		perscal = this.jd_to_persian(j);
		weekday = this.jwday(j);
		return new Array(perscal[0], perscal[1], perscal[2],weekday);
		},
	//  JD_TO_PERSIAN  --  Calculate Persian date from Julian day
		jd_to_persian: function (jd){
		var year, month, day, depoch, cycle, cyear, ycycle, aux1, aux2, yday;
		jd = Math.floor(jd) + 0.5;
		depoch = jd - this.persian_to_jd(475, 1, 1);
		cycle = Math.floor(depoch / 1029983);
		cyear = this._mod(depoch, 1029983);
		if (cyear == 1029982) {
		   ycycle = 2820;
		} else {
		   aux1 = Math.floor(cyear / 366);
		   aux2 = this._mod(cyear, 366);
		   ycycle = Math.floor(((2134 * aux1) + (2816 * aux2) + 2815) / 1028522) + aux1 + 1;
		}
		year = ycycle + (2820 * cycle) + 474;
		if (year <= 0) {
		year--;
		}
		yday = (jd - this.persian_to_jd(year, 1, 1)) + 1;
		month = (yday <= 186) ? Math.ceil(yday / 31) : Math.ceil((yday - 6) / 30);
		day = (jd - this.persian_to_jd(year, month, 1)) + 1;

		return new Array(year, month-1, day);
		},
		// PERSIAN_TO_JD  --  Determine Julian day from Persian date
		persian_to_jd: function (year, month, day){
		var epbase, epyear;
		epbase = year - ((year >= 0) ? 474 : 473);
		epyear = 474 + this._mod(epbase, 2820);
		return day +((month <= 7) ?((month - 1) * 31) :(((month - 1) * 30) + 6)) + Math.floor(((epyear * 682) - 110) / 2816) +(epyear - 1) * 365 + Math.floor(epbase / 2820) * 1029983 +(this._PERSIAN_EPOCH - 1);
		},
	//  GREGORIAN_TO_JD  --  Determine Julian day number from Gregorian calendar date
		gregorian_to_jd: function (year, month, day){
		    return (this._GREGORIAN_EPOCH - 1) + (365 * (year - 1)) + Math.floor((year - 1) / 4) +(-Math.floor((year - 1) / 100)) + Math.floor((year - 1) / 400) + Math.floor((((367 * month) - 362) / 12) +
		           ((month <= 2) ? 0 :(this.leap_gregorian(year) ? -1 : -2)) +day);
		},
		
	//  JD_TO_GREGORIAN  --  Calculate Gregorian calendar date from Julian day
		jd_to_gregorian : function (jd,jmonth) {
		var wjd, depoch, quadricent, dqc, cent, dcent, quad, dquad, yindex, dyindex, year, yearday, leapadj;
		wjd = Math.floor(jd - 0.5) + 0.5;
		depoch = wjd - this._GREGORIAN_EPOCH;
		quadricent = Math.floor(depoch / 146097);
		dqc = this._mod(depoch, 146097);
		cent = Math.floor(dqc / 36524);
		dcent = this._mod(dqc, 36524);
		quad = Math.floor(dcent / 1461);
		dquad = this._mod(dcent, 1461);
		yindex = Math.floor(dquad / 365);
		year = (quadricent * 400) + (cent * 100) + (quad * 4) + yindex;
		if (!((cent == 4) || (yindex == 4))) {
		    year++;
		}
		yearday = wjd - this.gregorian_to_jd(year, 1, 1);
		leapadj = ((wjd < this.gregorian_to_jd(year, 3, 1)) ? 0:(this.leap_gregorian(year) ? 1 : 2));
		month = Math.floor((((yearday + leapadj) * 12) + 373) / 367);
		day = (wjd - this.gregorian_to_jd(year, month, 1)) + 1;
		return new Array(year, month, day);
		},	valueOf:function(){
		// summary:
		//		This function returns The stored time value in milliseconds
		//		since midnight, January 1, 1970 UTC

		return this.toGregorian().valueOf();
	},jwday: function (j)
	{
		 return this._mod(Math.floor((j + 1.5)), 7);
		},

	// ported from the Java class com.ibm.icu.util.PersianCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
	_yearStart:function(/*Number*/year){
		// summary:
		//		return start of Persian year
		return (year-1)*354 + Math.floor((3+11*year)/30.0);
	},

	// ported from the Java class com.ibm.icu.util.PersianCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
	_monthStart:function(/*Number*/year, /*Number*/month){
		// summary:
		//		return the start of Persian Month
		return Math.ceil(29.5*month) +
			(year-1)*354 + Math.floor((3+11*year)/30.0);
	},
//  LEAP_GREGORIAN  --  Is a given year in the Gregorian calendar a leap year ?
	leap_gregorian: function (year)
	{
	    return ((year % 4) == 0) &&
	            (!(((year % 100) == 0) && ((year % 400) != 0)));
	},

//  LEAP_PERSIAN  --  Is a given year a leap year in the Persian calendar ?
	isLeapYear:function(j_y,j_m,j_d){
		// summary:
		//		return Boolean value if Persian leap year
		return !(j_y < 0 || j_y > 32767 || j_m < 1 || j_m > 12 || j_d < 1 || j_d >(this.daysInMonth[j_m-1] + (j_m == 12 && !((j_y-979)%33%4))));

	},

	// ported from the Java class com.ibm.icu.util.PersianCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
	getDaysInPersianMonth:function(/*Number*/month, /*Number*/ year){
		// summary:
		//		returns the number of days in the given Persian Month
		var days=this.daysInMonth[month];
		if(month==11 && this.isLeapYear(year,month+1,30)){
			days++;
		}
		return days;
	},

	_mod:function(a, b){
		return a - (b * Math.floor(a / b));
	}
});


IDate.getDaysInPersianMonth = function(/*dojox/date/persian.Date*/month){
	return new IDate().getDaysInPersianMonth(month.getMonth(),month.getFullYear()); // dojox.date.persian.Date
};
return IDate;
});
