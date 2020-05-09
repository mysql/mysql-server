define(["doh", "../time", "dojo/date", "dojo/date/locale", "dojox/date/hebrew/Date", "dojox/date/hebrew", "dojox/date/hebrew/locale", "dojox/calendar/time"],
	function(doh, time, date, dateLocale, hDate, h, hLocale, time){
	doh.register("tests.unitTest_Time", [
		function test_decodeDate(doh){
			var d = new Date(2009, 2, 20, 5, 27, 30, 0);
			var t = d.getTime();
			var hd = new hDate(t);
			var s = "2009-03-20T05:27:30";
			
			doh.is(date.compare(d, time.newDate(d)), 0);
			doh.is(date.compare(d, time.newDate(t)), 0);
			doh.is(date.compare(d, time.newDate(s)), 0);
			doh.is(date.compare(d, time.newDate(hd)), 0);
			
			doh.is(h.compare(hd, time.newDate(hd, hDate)), 0);
			doh.is(h.compare(hd, time.newDate(d, hDate)), 0);
			doh.is(h.compare(hd, time.newDate(t, hDate)), 0);
			doh.is(h.compare(hd, time.newDate(s, hDate)), 0);
			
		},
		
		function test_firstDayOfWeek_sun(doh){
			var weekdays = [
			  new Date(2013, 5, 2),
			  new Date(2013, 5, 3),
			  new Date(2013, 5, 4),
			  new Date(2013, 5, 5),
			  new Date(2013, 5, 6),
			  new Date(2013, 5, 7),
			  new Date(2013, 5, 8)
			];
			
			var fd = new Date(2013, 5, 2);
			
			doh.is(date.compare(fd, time.floorToWeek(weekdays[0], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[1], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[2], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[3], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[4], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[5], null, null, 0)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[6], null, null, 0)), 0);
			
		},
		
		function test_firstDayOfWeek_mon(doh){
			var weekdays = [
			  new Date(2013, 5, 3),
			  new Date(2013, 5, 4),
			  new Date(2013, 5, 5),
			  new Date(2013, 5, 6),
			  new Date(2013, 5, 7),
			  new Date(2013, 5, 8),
			  new Date(2013, 5, 9)
			];
			
			var fd = new Date(2013, 5, 3);
			
			doh.is(date.compare(fd, time.floorToWeek(weekdays[0], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[1], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[2], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[3], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[4], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[5], null, null, 1)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[6], null, null, 1)), 0);
			
		},
		
		function test_firstDayOfWeek_sat(doh){
			var weekdays = [
  			  new Date(2013, 5, 1),
			  new Date(2013, 5, 2),
			  new Date(2013, 5, 3),
			  new Date(2013, 5, 4),
			  new Date(2013, 5, 5),
			  new Date(2013, 5, 6),
			  new Date(2013, 5, 7)
			];
			
			var fd = new Date(2013, 5, 1);
			
			doh.is(date.compare(fd, time.floorToWeek(weekdays[0], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[1], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[2], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[3], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[4], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[5], null, null, 6)), 0);
			doh.is(date.compare(fd, time.floorToWeek(weekdays[6], null, null, 6)), 0);
			
		}

	]);
});
