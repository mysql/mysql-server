define([
	'intern!object',
	'intern/chai!assert',
	'dojo/request/registry',
	'dojo/_base/lang',
	'dojo/when',
	'../DynamoDB'
], function (
	registerSuite,
	assert,
	registry,
	lang,
	when,
	DynamoDB
) {
	var handles = [];
	var store;

	var testData = [
		{
			id: { S: '10001' },
			someMap: {
				M: {
					stringProperty: { S: 'Sam Smith' },
					listProperty: { L: [
						{ S: 'foo' },
						{ N: '2' },
						{ BOOL: false }
					] },
					nullProperty: { NULL: true }
				}
			},
			someBool: { BOOL: false },
			someList: { L: [
				{ S: 'bar' },
				{ NULL: true }
			] }
		},
		{
			id: { S: '10002' },
			someMap: {
				M: {
					stringProperty: { S: 'Bob Bobson' },
					listProperty: { L: [
						{ S: 'bar' },
						{ N: '3' },
						{ BOOL: true }
					] }
				}
			},
			someBool: { BOOL: true },
			someList: { L: [
				{ S: 'baz' },
				{ L: [ { N: 1 }, { N: 2 } ] }
			] }
		}
	];

	registerSuite({
		name: 'store/DynamoDB',

		beforeEach: function () {
			store = new DynamoDB({
				endpointUrl: 'endpoint',
				region: 'us-east-1'
			});
		},

		afterEach: function () {
			var handle;
			while ((handle = handles.pop())) {
				handle.remove();
			}
		},

		// check that store accesses expected endpoint URL
		endpointUrl: {
			generated: function () {
				var region = 'foo';
				var dfd = this.async();
				store = new DynamoDB({ region: region });

				handles.push(registry.register(/foo/, function (url) {
					dfd.callback(function () {
						assert.equal(url, 'https://dynamodb.' + region + '.amazonaws.com');
					})();
					return when({});
				}));

				store.get('one');
			},

			specified: function () {
				var dfd = this.async();
				store = new DynamoDB({ endpointUrl: 'foo' });

				handles.push(registry.register(/foo/, function (url) {
					dfd.callback(function () {
						assert.equal(url, 'foo');
					})();
					return when({});
				}));

				store.get('one');
			}
		},

		query: (function () {
			var countRequests;
			var dataRequests;

			function registerRequestHandler(dfd, countHandler, dataHandler) {
				handles.push(registry.register(store.endpointUrl, function (url, options) {
					var returnValue;
					dfd.callback(function () {
						assert.match(options.headers['x-amz-target'], /\.Query$/);
						var data = JSON.parse(options.data);

						if (data.Select === 'COUNT') {
							countRequests.push(data);
							returnValue = countHandler(data);
						}
						else {
							dataRequests.push(data);
							returnValue = dataHandler(data);
						}
					})();
					return when(returnValue);
				}));
			}

			return {
				beforeEach: function () {
					countRequests = [];
					dataRequests = [];
				},

				small: function () {
					// The deferred should resolve 3 times -- two calls to the endpoint and when the query promise
					// resolves
					var dfd = this.async(5000, 3);

					registerRequestHandler(dfd, function () {
						return { Count: testData.length, ScannedCount: testData.length };
					}, function () {
						return {
							Items: testData,
							Count: testData.length,
							ScannedCount: testData.length
						};
					});

					store.query().then(dfd.callback(function (items) {
						assert.lengthOf(countRequests, 1);
						assert.deepEqual(countRequests[0], {
							Select: 'COUNT',
							ConsistentRead: false,
							KeyConditions: {},
							TableName: store.tableName
						});
						assert.lengthOf(dataRequests, 1);
						assert.deepEqual(dataRequests[0], {
							ConsistentRead: false,
							KeyConditions: {},
							TableName: store.tableName
						});
						assert.lengthOf(items, testData.length);
					}));
				},

				large: function () {
					// For a large request, the server will return a LastEvaluatedKey entry that contains the ID of the
					// last key returned in the resultset. To get more data, the store must make an additional request,
					// providing the LastEvaluatedKey from the previous request as the ExclusiveStartKey for the next
					// request.

					// The deferred should resolve 4 times -- three calls to the endpoint and when the query promise
					// resolves
					var dfd = this.async(5000, 4);

					registerRequestHandler(dfd, function () {
						return { Count: 2, ScannedCount: 2 };
					}, function (data) {
						if (data.ExclusiveStartKey) {
							return {
								Items: [ testData[1] ],
								Count: 1,
								ScannedCount: 1
							};
						}
						else {
							return {
								Items: [ testData[0] ],
								Count: 1,
								ScannedCount: 1,
								LastEvaluatedKey: {
									id: testData[0].id
								}
							};
						}
					});

					store.query().then(dfd.callback(function (items) {
						assert.lengthOf(countRequests, 1);
						assert.deepEqual(countRequests[0], {
							Select: 'COUNT',
							ConsistentRead: false,
							KeyConditions: {},
							TableName: store.tableName
						});

						assert.lengthOf(dataRequests, 2);
						assert.deepEqual(dataRequests[0], {
							ConsistentRead: false,
							KeyConditions: {},
							TableName: store.tableName
						});
						assert.deepEqual(dataRequests[1], {
							ConsistentRead: false,
							KeyConditions: {},
							TableName: store.tableName,
							ExclusiveStartKey: {
								id: testData[0].id
							}
						});
						assert.lengthOf(items, testData.length);
					}));
				},

				filter: function () {
					// The deferred should resolve 3 times -- two calls to the endpoint and when the query promise
					// resolves
					var dfd = this.async(5000, 3);

					registerRequestHandler(dfd, function () {
						return { Count: testData.length, ScannedCount: testData.length };
					}, function () {
						return {
							Items: testData,
							Count: testData.length,
							ScannedCount: testData.length
						};
					});

					var expectedConditions = {
						ExpressionAttributeNames: {
							AttributeValueList: [
								{ M: { '#n': { S: 'name' } } }
							],
							ComparisonOperator: 'EQ'
						},
						ExpressionAttributeValues: {
							AttributeValueList: [
								{ M: { ':word': { S: 'Name' } } }
							],
							ComparisonOperator: 'EQ'
						},
						FilterExpression: {
							AttributeValueList: [
								{ S: 'contains(#n, :word)' }
							],
							ComparisonOperator: 'EQ'
						}
					};

					store.query({
						FilterExpression: 'contains(#n, :word)',
						ExpressionAttributeNames: { '#n': 'name' },
						ExpressionAttributeValues: { ':word': 'Name' }
					}).then(dfd.callback(function () {
						assert.lengthOf(countRequests, 1);
						assert.deepEqual(countRequests[0], {
							Select: 'COUNT',
							ConsistentRead: false,
							KeyConditions: expectedConditions,
							TableName: store.tableName
						});
						assert.lengthOf(dataRequests, 1);
						assert.deepEqual(dataRequests[0], {
							ConsistentRead: false,
							KeyConditions: expectedConditions,
							TableName: store.tableName
						});
					}));
				}
			};
		})(),

		get: function () {
			var dfd = this.async();
			var id = '123';

			handles.push(registry.register(store.endpointUrl, function (url, options) {
				dfd.callback(function () {
					assert.match(options.headers['x-amz-target'], /\.GetItem$/);
				})();
				return when({});
			}));

			store.get(id);
		},

		put: function () {
			var dfd = this.async();
			var item = {
				id: 1,
				stringValue: 'foo',
				boolValue: true
			};

			handles.push(registry.register(store.endpointUrl, function (url, options) {
				dfd.callback(function () {
					assert.match(options.headers['x-amz-target'], /\.PutItem$/);
					assert.deepEqual(JSON.parse(options.data), {
						Item: {
							id: { N: String(item.id) },
							stringValue: { S: item.stringValue },
							boolValue: { BOOL: item.boolValue }
						},
						TableName: store.tableName
					});
				})();
				return when({});
			}));

			store.put(item);
		},

		remove: function () {
			var dfd = this.async();
			var removeId = '123';

			handles.push(registry.register(store.endpointUrl, function (url, options) {
				dfd.callback(function () {
					assert.match(options.headers['x-amz-target'], /\.DeleteItem$/);
					assert.deepEqual(JSON.parse(options.data), {
						Key: { id: { S: removeId } },
						ReturnValues: 'ALL_OLD',
						TableName: store.tableName
					});
				})();
				return when({});
			}));

			store.remove(removeId);
		}, 

		sign: (function () {
			var request;
			var headers;
			var date = new Date(2006, 0, 2, 15, 4, 5, 6);

			return {
				beforeEach: function () {
					headers = {};
					request = {
						body: 'foo',
						host: 'bar',
						method: 'POST',
						headers: headers
					};
					store.credentials = {
						AccessKeyId: 'ABC123',
						SecretAccessKey: 'DEF456'
					};
				},

				'without token': function () {
					store._signRequest(request, date);
					assert.deepEqual(headers, {
						'x-amz-date': '20060102T200405Z',
						// jshint maxlen: 250
						'authorization': 'AWS4-HMAC-SHA256 Credential=ABC123/20060102/us-east-1/dynamodb/aws4_request,SignedHeaders=host;x-amz-date;x-amz-target,Signature=10ae50b25e9adafe66134589a071bfd29e464850cf1908e99ad722edd9e962f7'
					});
				},

				'with token': function () {
					store.credentials.SessionToken = 'GHI789';
					store._signRequest(request, date);
					assert.deepEqual(headers, {
						'x-amz-date': '20060102T200405Z',
						'x-amz-security-token': 'GHI789',
						// jshint maxlen: 280
						'authorization': 'AWS4-HMAC-SHA256 Credential=ABC123/20060102/us-east-1/dynamodb/aws4_request,SignedHeaders=host;x-amz-date;x-amz-target;x-amz-security-token,Signature=df743b4f3447ba764f55eb387a42090bd62c0a55be0298535039d2ee3ff9d78b'
					});
				}
			};
		})()
	});
});
