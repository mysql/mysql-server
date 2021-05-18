define("dojox/date/umalqura/Date", ["dojo/_base/lang", "dojo/_base/declare", "dojo/date", "../islamic/Date"],
 function(lang, declare, dd){

	var IDate = declare("dojox.date.umalqura.Date", null, {	
	
	// summary:
	//		The component defines the UmAlqura (Hijri) Calendar Object according to Umalqura calculations
    //		This module is similar to the Date() object provided by JavaScript
    // example:
    // |	var date = new dojox.date.umalqura.Date();
    // |	document.writeln(date.getFullYear()+'\'+date.getMonth()+'\'+date.getDate());

    _MONTH_LENGTH: [
    //1300-1304
    "101010101010", "110101010100", "111011001001", "011011010100", "011011101010",
    //1305-1309
    "001101101100", "101010101101", "010101010101", "011010101001", "011110010010",
    //1310-1314
    "101110101001", "010111010100", "101011011010", "010101011100", "110100101101",
    //1315-1319
    "011010010101", "011101001010", "101101010100", "101101101010", "010110101101",
    //1320-1324
    "010010101110", "101001001111", "010100010111", "011010001011", "011010100101",
    //1325-1329
    "101011010101", "001011010110", "100101011011", "010010011101", "101001001101",
    //1330-1334
    "110100100110", "110110010101", "010110101100", "100110110110", "001010111010",
    //1335-1339
    "101001011011", "010100101011", "101010010101", "011011001010", "101011101001",
    //1340-1344
    "001011110100", "100101110110", "001010110110", "100101010110", "101011001010",
    //1345-1349
    "101110100100", "101111010010", "010111011001", "001011011100", "100101101101",
    //1350-1354
    "010101001101", "101010100101", "101101010010", "101110100101", "010110110100",
    //1355-1359
    "100110110110", "010101010111", "001010010111", "010101001011", "011010100011",
    //1360-1364
    "011101010010", "101101100101", "010101101010", "101010101011", "010100101011",
    //1365-1369
    "110010010101", "110101001010", "110110100101", "010111001010", "101011010110",
    //1370-1374
    "100101010111", "010010101011", "100101001011", "101010100101", "101101010010",
    //1375-1379
    "101101101010", "010101110101", "001001110110", "100010110111", "010001011011",
    //1380-1384
    "010101010101", "010110101001", "010110110100", "100111011010", "010011011101",
    //1385-1389
    "001001101110", "100100110110", "101010101010", "110101010100", "110110110010",
    //1390-1394
    "010111010101", "001011011010", "100101011011", "010010101011", "101001010101",
    //1395-1399
    "101101001001", "101101100100", "101101110001", "010110110100", "101010110101",
    //1400-1404
    "101001010101", "110100100101", "111010010010", "111011001001", "011011010100",
    //1405-1409
    "101011101001", "100101101011", "010010101011", "101010010011", "110101001001",
    //1410-1414
    "110110100100", "110110110010", "101010111001", "010010111010", "101001011011",
    //1415-1419
    "010100101011", "101010010101", "101100101010", "101101010101", "010101011100",
    //1420-1424
    "010010111101", "001000111101", "100100011101", "101010010101", "101101001010",
    //1425-1429
    "101101011010", "010101101101", "001010110110", "100100111011", "010010011011",
    //1430-1434
    "011001010101", "011010101001", "011101010100", "101101101010", "010101101100",
    //1435-1439
    "101010101101", "010101010101", "101100101001", "101110010010", "101110101001",
    //1440-1444
    "010111010100", "101011011010", "010101011010", "101010101011", "010110010101",
    //1445-1449
    "011101001001", "011101100100", "101110101010", "010110110101", "001010110110",
    //1450-1454
    "101001010110", "111001001101", "101100100101", "101101010010", "101101101010",
    //1455-1459
    "010110101101", "001010101110", "100100101111", "010010010111", "011001001011",
    //1460-1464
    "011010100101", "011010101100", "101011010110", "010101011101", "010010011101",
    //1465-1469
    "101001001101", "110100010110", "110110010101", "010110101010", "010110110101",
    //1470-1474
    "001011011010", "100101011011", "010010101101", "010110010101", "011011001010",
    //1475-1479
    "011011100100", "101011101010", "010011110101", "001010110110", "100101010110",
    //1480-1484
    "101010101010", "101101010100", "101111010010", "010111011001", "001011101010",
    //1485-1489
    "100101101101", "010010101101", "101010010101", "101101001010", "101110100101",
    //1490-1494
    "010110110010", "100110110101", "010011010110", "101010010111", "010101000111",
    //1495-1499
    "011010010011", "011101001001", "101101010101", "010101101010", "101001101011",
    //1500-1504
    "010100101011", "101010001011", "110101000110", "110110100011", "010111001010",
    //1505-1509
    "101011010110", "010011011011", "001001101011", "100101001011", "101010100101",
    //1510-1514
    "101101010010", "101101101001", "010101110101", "000101110110", "100010110111",
    //1515-1519
    "001001011011", "010100101011", "010101100101", "010110110100", "100111011010",
    //1520-1524
    "010011101101", "000101101101", "100010110110", "101010100110", "110101010010",
    //1525-1529
    "110110101001", "010111010100", "101011011010", "100101011011", "010010101011",
    //1530-1534
    "011001010011", "011100101001", "011101100010", "101110101001", "010110110010",
    //1535-1539
    "101010110101", "010101010101", "101100100101", "110110010010", "111011001001",
    //1540-1544
    "011011010010", "101011101001", "010101101011", "010010101011", "101001010101",
    //1545-1549
    "110100101001", "110101010100", "110110101010", "100110110101", "010010111010",
    //1550-1554
    "101000111011", "010010011011", "101001001101", "101010101010", "101011010101",
    //1555-1559
    "001011011010", "100101011101", "010001011110", "101000101110", "110010011010",
    //1560-1564
    "110101010101", "011010110010", "011010111001", "010010111010", "101001011101",
    //1565-1569
    "010100101101", "101010010101", "101101010010", "101110101000", "101110110100",
    //1570-1574
    "010110111001", "001011011010", "100101011010", "101101001010", "110110100100",
    //1575-1579
    "111011010001", "011011101000", "101101101010", "010101101101", "010100110101",
    //1580-1584
    "011010010101", "110101001010", "110110101000", "110111010100", "011011011010",
    //1585-1589
    "010101011011", "001010011101", "011000101011", "101100010101", "101101001010",
    //1590-1594
    "101110010101", "010110101010", "101010101110", "100100101110", "110010001111",
    //1595-1599
    "010100100111", "011010010101", "011010101010", "101011010110", "010101011101",
    //1600
    "001010011101"],

    _hijriBegin: 1300,
    _hijriEnd: 1600,
    _date: 0,
    _month: 0,
    _year: 0,
    _hours: 0,
    _minutes: 0,
    _seconds: 0,
    _milliseconds: 0,
    _day: 0,

    constructor: function(){
        // summary:
        //		This function initialize the date object values

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
            }else{  // this is umalqura.Date object
                this._year = arg0._year;
                this._month = arg0._month;
                this._date = arg0._date;
                this._hours = arg0._hours;
                this._minutes = arg0._minutes;
                this._seconds = arg0._seconds;
                this._milliseconds = arg0._milliseconds;
            }
        }else if(len >= 3){
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

    getDate: function(){
        // summary:
        //		This function returns the date value (1 - 30)
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |
        // |	document.writeln(date1.getDate);
        return this._date;
    },

    getMonth: function(){
        // summary:
        //		This function return the month value ( 0 - 11 )
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |
        // |	document.writeln(date1.getMonth()+1);

        return this._month;
    },

    getFullYear: function(){
        // summary:
        //		This function return the year value
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |
        // |	document.writeln(date1.getFullYear());

        return this._year;
    },

    getDay: function(){
        // summary:
        //		This function returns the week day value ( 0 - 6 )
        //		sunday is 0, monday is 1,...etc
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |
        // |	document.writeln(date1.getDay());
        var d = this.toGregorian();
        var dd = d.getDay();
        return dd;
    },

    getHours: function(){
        // summary:
        //		returns the hour value
        return this._hours;
    },

    getMinutes: function(){
        // summary:
        //		returns the minutes value
        return this._minutes;
    },

    getSeconds: function(){
        // summary:
        //		returns the seconds value
        return this._seconds;
    },

    getMilliseconds: function(){
        // summary:
        //		returns the milliseconds value
        return this._milliseconds;
    },

    setDate: function(/*number*/ date){
        // summary:
        //		This function sets the date
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |	date1.setDate(2);

        date = parseInt(date);

        if(date > 0 && date <= this.getDaysInIslamicMonth(this._month, this._year)){
            this._date = date;
        }else{
            var mdays;
            if(date > 0){
                for(mdays = this.getDaysInIslamicMonth(this._month, this._year);
					date > mdays;
						date -= mdays, mdays = this.getDaysInIslamicMonth(this._month, this._year)){
                    this._month++;
                    if(this._month >= 12){ this._year++; this._month -= 12; }
                }

                this._date = date;
            }else{
                for(mdays = this.getDaysInIslamicMonth((this._month - 1) >= 0 ? (this._month - 1) : 11, ((this._month - 1) >= 0) ? this._year : this._year - 1);
						date <= 0;
							mdays = this.getDaysInIslamicMonth((this._month - 1) >= 0 ? (this._month - 1) : 11, ((this._month - 1) >= 0) ? this._year : this._year - 1)){
                    this._month--;
                    if(this._month < 0){ this._year--; this._month += 12; }

                    date += mdays;
                }
                this._date = date;
            }
        }
        return this;
    },

    setFullYear: function(/*number*/ year){
        // summary:
        //		This function set Year
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |	date1.setYear(1429);

        this._year = +year;
    },

    setMonth: function(/*number*/ month){
        // summary:
        //		This function sets the month
        // example:
        // |	var date1 = new dojox.date.umalqura.Date();
        // |	date1.setMonth(2);

        this._year += Math.floor(month / 12);
        if(month > 0){
            this._month = Math.floor(month % 12);
        }else{
            this._month = Math.floor(((month % 12) + 12) % 12);
        }
    },

    setHours: function(){
        // summary:
        //		set the hours
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

        while (hours >= 24){
            this._date++;
            var mdays = this.getDaysInIslamicMonth(this._month, this._year);
            if(this._date > mdays){
                this._month++;
                if(this._month >= 12){ this._year++; this._month -= 12; }
                this._date -= mdays;
            }
            hours -= 24;
        }
        this._hours = hours;
    },

	_addMinutes: function(/*Number*/ minutes){
		minutes += this._minutes;
		this.setMinutes(minutes);
		this.setHours(this._hours + parseInt(minutes / 60));
		return this;
	},

	_addSeconds: function(/*Number*/ seconds){
		seconds += this._seconds;
		this.setSeconds(seconds);
		this._addMinutes(parseInt(seconds / 60));
		return this;
	},

	_addMilliseconds: function(/*Number*/ milliseconds){
		milliseconds += this._milliseconds;
		this.setMilliseconds(milliseconds);
		this._addSeconds(parseInt(milliseconds / 1000));
		return this;
	},

    setMinutes: function(/*number*/ minutes){
        // summary:
        //		set the minutes

        while (minutes >= 60){
            this._hours++;
            if(this._hours >= 24){
                this._date++;
                this._hours -= 24;
                var mdays = this.getDaysInIslamicMonth(this._month, this._year);
                if(this._date > mdays){
                    this._month++;
                    if(this._month >= 12){ this._year++; this._month -= 12; }
                    this._date -= mdays;
                }
            }
            minutes -= 60;
        }
        this._minutes = minutes;
    },


    setSeconds: function(/*number*/ seconds){
        // summary:
        //		set seconds
        while (seconds >= 60){
            this._minutes++;
            if(this._minutes >= 60){
                this._hours++;
                this._minutes -= 60;
                if(this._hours >= 24){
                    this._date++;
                    this._hours -= 24;
                    var mdays = this.getDaysInIslamicMonth(this._month, this._year);
                    if(this._date > mdays){
                        this._month++;
                        if(this._month >= 12){ this._year++; this._month -= 12; }
                        this._date -= mdays;
                    }
                }
            }
            seconds -= 60;
        }
        this._seconds = seconds;
    },

    setMilliseconds: function(/*number*/ milliseconds){
        // summary:
        //		set the milliseconds
        while (milliseconds >= 1000){
            this.setSeconds++;
            if(this.setSeconds >= 60){
                this._minutes++;
                this.setSeconds -= 60;
                if(this._minutes >= 60){
                    this._hours++;
                    this._minutes -= 60;
                    if(this._hours >= 24){
                        this._date++;
                        this._hours -= 24;
                        var mdays = this.getDaysInIslamicMonth(this._month, this._year);
                        if(this._date > mdays){
                            this._month++;
                            if(this._month >= 12){ this._year++; this._month -= 12; }
                            this._date -= mdays;
                        }
                    }
                }
            }
            milliseconds -= 1000;
        }
        this._milliseconds = milliseconds;
    },


    toString: function(){
        // summary:
        //		This returns a string representation of the date in "DDDD MMMM DD YYYY HH:MM:SS" format
        // example:
        // |		var date1 = new dojox.date.umalqura.Date();
        // |		document.writeln(date1.toString());

        //FIXME: TZ/DST issues?
        var x = new Date();
        x.setHours(this._hours);
        x.setMinutes(this._minutes);
        x.setSeconds(this._seconds);
        x.setMilliseconds(this._milliseconds);
        return this._month + " " + this._date + " " + this._year + " " + x.toTimeString();
    },


    toGregorian: function(){
        // summary:
        //		This returns the equivalent gregorian date value in Date object
        // example:
        // |	var dateIslamic = new dojox.date.umalqura.Date(1429,11,20);
        // |	var dateGregorian = dateIslamic.toGregorian();

        var hYear = this._year;
        var hMonth = this._month;
        var hDate = this._date;
        var gdate = new Date();
        var dayDiff = hDate - 1;
		var gregorianFirstRef = new Date(1882, 10, 12, this._hours, this._minutes, this._seconds, this._milliseconds);
        if(hYear >= this._hijriBegin && hYear <= this._hijriEnd){
			for (var y = 0; y <  hYear - this._hijriBegin; y++){
				for(var m = 0; m < 12; m++){
					dayDiff += parseInt(this._MONTH_LENGTH[y][m], 10) + 29;
				}
			}
			for(m = 0; m < hMonth; m++){
				dayDiff += parseInt(this._MONTH_LENGTH[hYear - this._hijriBegin][m], 10) + 29;
			}
			gdate = dd.add(gregorianFirstRef, "day", dayDiff);
			
        } else{
            var islamicDate = new dojox.date.islamic.Date(this._year, this._month, this._date, this._hours, this._minutes, this._seconds, this._milliseconds);
            gdate = new Date(islamicDate.toGregorian());
        }
        return gdate;
    },

	getDaysDiff: function (date1, date2) {
        // summary:
        //      This function returns the number of days between two different dates.
        var ONE_DAY = 1000 * 60 * 60 * 24;
        var diff = Math.abs(date1.getTime() - date2.getTime());
        return Math.round(diff / ONE_DAY);
    },
	
    //TODO: would it make more sense to make this a constructor option? or a static?
    fromGregorian: function(/*Date*/ gdate){
        // summary:
        //		This function returns the equivalent UmAlqura Date value for the Gregorian Date
        // example:
        // |		var dateIslamic = new dojox.date.umalqura.Date();
        // |		var dateGregorian = new Date(2008,10,12);
        // |		dateIslamic.fromGregorian(dateGregorian);

        var date = new Date(gdate);
        var gregorianFirstRef = new Date(1882, 10, 12, 0, 0, 0, 0);
        var gregorianLastRef = new Date(2174, 10, 25, 23, 59, 59, 999);
        var daysDiff = this.getDaysDiff(date, gregorianFirstRef);
        if (date - gregorianFirstRef >= 0 && date - gregorianLastRef <= 0) {
            var year = 1300;
            for (var i = 0; i < this._MONTH_LENGTH.length; i++, year++) {
                for (var j = 0; j < 12; j++) {
                    var numOfDays = parseInt(this._MONTH_LENGTH[i][j], 10) + 29;
                    if (daysDiff <= numOfDays) {
                        this._date = daysDiff + 1;
                        if (this._date > numOfDays) {
                            this._date = 1;
                            j++;
                        }
                        if (j > 11) {
                            j = 0;
                            year++;
                        }
                        this._month = j;
                        this._year = year;
                        this._hours = date.getHours();
                        this._minutes = date.getMinutes();
                        this._seconds = date.getSeconds();
                        this._milliseconds = date.getMilliseconds();
                        this._day = date.getDay();
                        return this;
                    }
                    daysDiff = parseInt(daysDiff, 10) - numOfDays;
                }
            }
        } else {
            var islamicDate = new dojox.date.islamic.Date(date);
            this._date = islamicDate.getDate();
			this._month = islamicDate.getMonth();
			this._year = islamicDate.getFullYear();
			this._hours = gdate.getHours();
			this._minutes = gdate.getMinutes();
			this._seconds = gdate.getSeconds();
			this._milliseconds = gdate.getMilliseconds();
			this._day = gdate.getDay();
        }
        return this;
    },

    valueOf: function(){
        // summary:
        //		This function returns the stored time value in milliseconds
        //		since midnight, January 1, 1970 UTC

        return (this.toGregorian()).valueOf();
    },

    // ported from the Java class com.ibm.icu.util.IslamicCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
    _yearStart: function(/*Number*/year){
        // summary:
        //		return start of Islamic year
        return (year - 1) * 354 + Math.floor((3 + 11 * year) / 30.0); //1078
    },

    // ported from the Java class com.ibm.icu.util.IslamicCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
    _monthStart: function(/*Number*/year, /*Number*/month){
        // summary:
        //		return the start of Islamic Month
        return Math.ceil(29.5 * month) +
			(year - 1) * 354 + Math.floor((3 + 11 * year) / 30.0);
    },

    // ported from the Java class com.ibm.icu.util.IslamicCalendar from ICU4J v3.6.1 at http://www.icu-project.org/
    _civilLeapYear: function(/*Number*/year){
        // summary:
        //		return Boolean value if Islamic leap year
        return (14 + 11 * year) % 30 < 11;
    },


    getDaysInIslamicMonth: function(/*Number*/ month, /*Number*/ year){
        // summary:
        //		returns the number of days in the given Islamic month
        if(year >= this._hijriBegin && year <= this._hijriEnd){
            var pos = year - this._hijriBegin;
            var length = 0;
            if(this._MONTH_LENGTH[pos].charAt(month) == 1)
                length = 30;
            else length = 29;
        }else{
            var islamicDate = new dojox.date.islamic.Date();
            length = islamicDate.getDaysInIslamicMonth(month, year);
        }
        return length;
    }
});

//TODOC
IDate.getDaysInIslamicMonth = function(/*dojox.date.umalqura.Date*/month){
	return new IDate().getDaysInIslamicMonth(month.getMonth(),month.getFullYear()); // dojox.date.islamic.Date
};

return IDate;
});

