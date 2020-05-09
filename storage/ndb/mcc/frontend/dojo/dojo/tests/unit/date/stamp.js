define([
	'intern!object',
	'intern/chai!assert',
	'../../../date/stamp'
], function (registerSuite, assert, stamp) {
	registerSuite({
		name: 'dojo/date/stamp',

		'.fromISOString': {
			// Accepts a string formatted according to a profile of ISO8601 as defined by
			// [RFC3339](http://www.ietf.org/rfc/rfc3339.txt), except that partial input is allowed.
			'ISO8601 & RFC3339': {
				'date and time with timezone offset': function () {
					var rfc = '2005-06-29T08:05:00-07:00';
					var date = stamp.fromISOString(rfc);
					assert.equal(2005, date.getFullYear());
					assert.equal(5, date.getMonth());
					assert.equal(29, date.getUTCDate());
					assert.equal(15, date.getUTCHours());
					assert.equal(5, date.getUTCMinutes());
					assert.equal(0, date.getSeconds());
				},

				'date only': function () {
					var rfc = '2004-02-29';
					var date = stamp.fromISOString(rfc);
					assert.equal(2004, date.getFullYear());
					assert.equal(1, date.getMonth());
					assert.equal(29, date.getDate());
				},

				'year and month only': function () {
					var rfc = '2004-01';
					var date = stamp.fromISOString(rfc);
					assert.equal(2004, date.getFullYear());
					assert.equal(0, date.getMonth());
					assert.equal(1, date.getDate());
				},

				'date and time without timezone': function () {
					// No TZ info means local time
					var rfc = '2004-02-29T01:23:45';
					var date = stamp.fromISOString(rfc);
					assert.equal(2004, date.getFullYear());
					assert.equal(1, date.getMonth());
					assert.equal(29, date.getDate());
					assert.equal(1, date.getHours());
				},

				'pre-epoch date': function () {
					var rfc = '0101-01-01';
					var date = stamp.fromISOString(rfc);
					assert.equal(101, date.getFullYear());
					assert.equal(0, date.getMonth());
					assert.equal(1, date.getDate());
				},

				'pre-epoch date and time': function () {
					var rfc = '0001-01T00:00:00';
					var date = stamp.fromISOString(rfc);
					assert.equal(1, date.getFullYear());
				},

				'time only': function () {
					var date = stamp.fromISOString('T18:46:39');
					assert.equal(18, date.getHours());
					assert.equal(46, date.getMinutes());
					assert.equal(39, date.getSeconds());
				}
			},

			// process dates as specified [by the W3C](http://www.w3.org/TR/NOTE-datetime)
			'w3 date-time': {
				'timestamp only with timezone offset': function() {
					var date = stamp.fromISOString('T18:46:39+07:00');
					assert.equal(11, date.getUTCHours());
				},

				'timestamp only with 0 timezone offset and + designator': function () {
					var date = stamp.fromISOString('T18:46:39+00:00');
					assert.equal(18, date.getUTCHours());
				},

				'timestamp only with timezone desiginator and no offset': function () {
					var date = stamp.fromISOString('T18:46:39Z');
					assert.equal(18, date.getUTCHours());
				},

				'timestamp only with timzone offset with - designator': function () {
					var date = stamp.fromISOString('T16:46:39-07:00');
					assert.equal(23, date.getUTCHours());
				},

				'GMT timestamp only with timezone designator, no offset': function () {
					var date = stamp.fromISOString('T00:00:00Z', new Date(2010,3,1));
					assert.equal(0, date.getUTCHours());
					assert.equal(2010, date.getFullYear());
				}
			}
		},

		'.toISOString': {
			'year': function () {
				var date = new Date(2005, 5, 29, 8, 5, 0);
				var rfc = stamp.toISOString(date);
				//truncate for comparison
				assert.equal('2005-06', rfc.substring(0, 7));
			},

			'pre-epoch year': function () {
				var date = new Date(101, 0, 2);
				var rfc;

				date.setFullYear(101);
				rfc = stamp.toISOString(date);
				//truncate for comparison
				assert.equal('0101-01', rfc.substring(0, 7));
			}
		}
	});
});
