define([
	'intern!object',
	'intern/chai!assert',
	'../../io-query'
], function (registerSuite, assert, ioQuery) {
	var object1 = {
		'blah': 'blah'
	};
	var query1 = 'blah=blah';
	var object2 = {
		'blåh': 'bláh'
	};
	var query2 = 'bl%C3%A5h=bl%C3%A1h';
	var object3 = {
		blah: 'blah',
		multi: [
			'thud',
			'thonk'
		],
		textarea: 'textarea_value'
	};
	var query3 = 'blah=blah&multi=thud&multi=thonk&textarea=textarea_value';
	var object4 = {
		spaces: 'string with spaces'
	};
	var query4 = 'spaces=string%20with%20spaces';
	var query4_extra = query4 + '&';

	registerSuite({
		name: 'dojo/io-query',

		'.objectToQuery': function () {
			assert.strictEqual(ioQuery.objectToQuery(object1), query1);
			assert.strictEqual(ioQuery.objectToQuery(object2), query2);
			assert.strictEqual(ioQuery.objectToQuery(object3), query3);
			assert.strictEqual(ioQuery.objectToQuery(object4), query4);
		},
		'.queryToObject': function () {
			assert.deepEqual(ioQuery.queryToObject(query1), object1);
			assert.deepEqual(ioQuery.queryToObject(query2), object2);
			assert.deepEqual(ioQuery.queryToObject(query3), object3);
			assert.deepEqual(ioQuery.queryToObject(query4), object4);
			assert.deepEqual(ioQuery.queryToObject(query4_extra), object4);
		}
	});
});
