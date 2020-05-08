define([
	'dojo/_base/declare',
	'dojo/Stateful',
	'dojo/request',
	'dojo/store/util/QueryResults',
	'dojo/store/util/SimpleQueryEngine',
	'dojo/_base/lang',
	'dojo/_base/array',
	'dojo/errors/RequestError',
	'dojo/Deferred',
	'../encoding/digests/SHA256',
	'../encoding/digests/_base',
	'dojo/has!host-node?dojo/node!url'
], function (
	declare,
	Stateful,
	request,
	createQueryResults,
	SimpleQueryEngine,
	lang,
	arrayUtil,
	RequestError,
	Deferred,
	hash,
	digests,
	nodeUrl
) {
	//	summary:
	//		This module provides a `dojo/store` interface to an Amazon DynamoDB table. It must be configured for a
	//		specific AWS region, and the user must provide permanent or temporary authorization credentials to allow
	//		access to the table.
	//
	//		A typical store creation using temporary access credentials would look like:
	//		| var store new DynamoDB({
	//		|	tableName: 'myData',
	//		|	idProperty: 'id',
	//		|	region: 'us-east-1',
	//		|	credentials: {
	//		|		AccessKeyId: 'abc123',
	//		|		SecretAccessKey: 'def456',
	//		|		SessionToken: 'ghi6789'
	//		|	}
	//		| }));
	//
	//		Note that this module supports version 4 AWS signatures, which is the version supported by the AWS SDKs.
	//		See http://docs.aws.amazon.com/general/latest/gr/signing_aws_api_requests.html for additional information
	//		about AWS request signing.

	function hmac(key, data, outputType) {
		//	summary:
		//		Convenience function for computing the HMAC. Output is returned as a binary string by default to allow
		//		for successive calls.
		//	returns: string

		outputType = outputType || digests.outputTypes.String;
		return hash._hmac(data, key, outputType);
	}

	function getSigningKey(shortDate, secretKey, region) {
		//	summary:
		//		Get an AWS signing key for the current date.
		//	returns: string

		this._signingDate = shortDate;
		var kDate = hmac('AWS4' + secretKey, shortDate);
		var kRegion = hmac(kDate, region);
		var kService = hmac(kRegion, 'dynamodb');
		return hmac(kService, 'aws4_request');
	}

	function toUTCString(date) {
		//	summary:
		//		Convert a given date to a UTC datetime string.
		//	returns: string

		function twoDigit(value) {
			value = String(value);
			return value.length === 1 ? '0' + value : value;
		}
		var year = String(date.getUTCFullYear());
		var month = twoDigit(date.getUTCMonth() + 1);
		var day = twoDigit(date.getUTCDate());
		var hour = twoDigit(date.getUTCHours());
		var minute = twoDigit(date.getUTCMinutes());
		var second = twoDigit(date.getUTCSeconds());
		return year + month + day + 'T' + hour + minute + second + 'Z';
	}

	function getHostName(url) {
		//	summary:
		//		Extract the hostname from a URL.
		//	returns: string

		if (typeof document !== 'undefined') {
			var a = document.createElement('a');
			a.href = url;
			return a.hostname;
		}
		else {
			url = nodeUrl.parse(url);
			return url.hostname;
		}
	}

	function getDynamoType(value) {
		//	summary:
		//		Return the corresponding DynamoDB type for the given value
		//	returns: string

		if (value == null) {
			// return NULL for null or undefined values
			return 'NULL';
		}

		var type = typeof value;

		if (type === 'string') {
			return 'S';
		}

		if (type === 'boolean') {
			return 'BOOL';
		}

		if (type === 'number') {
			return 'N';
		}

		if (value instanceof Array) {
			// DynamoDB array equivalent is 'List'
			return 'L';
		}

		// default to 'Map', the DynamoDB equivalent to a JavaScript object
		return 'M';
	}

	function getDynamoValue(value) {
		//	summary:
		//		Convert a JavaScript value into a DynamoDB typed object.
		//	returns: Object

		var type = getDynamoType(value);
		var dynamoValue = {};
		var returnValue;

		switch (type) {
		case 'BOOL':
		case 'S':
			returnValue = value;
			break;
		case 'N':
			returnValue = String(value);
			break;
		case 'NULL':
			returnValue = true;
			break;
		case 'M':
			returnValue = {};
			for (var key in value) {
				returnValue[key] = getDynamoValue(value[key]);
			}
			break;
		case 'L':
			returnValue = [];
			for (var i = 0; i < value.length; i++) {
				returnValue[i] = getDynamoValue(value[i]);
			}
			break;
		default:
			throw new Error('Unknown Dynamo type: ' + type);
		}

		dynamoValue[type] = returnValue;
		return dynamoValue;
	}

	function getNativeValue(dynamoValue) {
		//	summary:
		//		Convert a DynamoDB value into a JavaScript value.
		//	returns: any

		var type;
		var value;

		// fill in the type and value variables from the dynamoValue, which is a { type: value } object
		for (type in dynamoValue) {
			value = dynamoValue[type];
		}

		function parse(value) {
			if (type === 'N') {
				return Number(value);
			}
			if (type === 'NULL') {
				return null;
			}
			if (type === 'L') {
				return arrayUtil.map(value, function (value) {
					return getNativeValue(value);
				});
			}
			if (type === 'M') {
				var returnValue = {};
				for (var key in value) {
					returnValue[key] = getNativeValue(value[key]);
				}
				return returnValue;
			}

			// strings, booleans, and binary data are returned as-is
			return value;
		}

		if (type.charAt(1) === 'S') {
			// type is a StringSet, NumberSet, or BinarySet -- set type to the contained type, parse each contained
			// element, and return in an array
			type = type.charAt(0);
			return arrayUtil.map(value, parse);
		}

		return parse(value);
	}

	function toDynamoObject(object) {
		//	summary:
		//		Convert a JavaScript object to a format consumable by DynamoDB.
		//	returns: Object

		var dynamoObject = {};
		for (var k in object) {
			dynamoObject[k] = getDynamoValue(object[k]);
		}
		return dynamoObject;
	}

	function fromDynamoObject(dynamoObject) {
		//	summary:
		//		Converts a DynamoDB hash map to one that conforms to normal JavaScript object conventions.
		//	returns: Object|null

		if (!dynamoObject) {
			return null;
		}

		var object = {};
		for (var k in dynamoObject) {
			object[k] = getNativeValue(dynamoObject[k]);
		}

		return object;
	}

	return declare(Stateful, {
		//	summary:
		//		DynamoDB provides a `dojo/store` interface to an Amazon DynamoDB table.

		//	tableName: string
		//		The name of the DynamoDB table being accessed by this store.
		tableName: '',

		//	attributesToGet: Array?
		//		An optional array of attribute key names that should be fetched when retrieving an object from the
		//		table. If `null`, all attributes will be retrieved.
		attributesToGet: null,

		//	consistentRead: boolean
		//		Whether or not to enforce consistent reads on the table.
		consistentRead: false,

		//	maxRetries: number
		//		The maximum number of times a request to the server should be retried before treating it as a failure.
		maxRetries: 5,

		//	idProperty: string|string[]
		//		The names of the property or properties that are used as the primary key within the DynamoDB. If using
		//		a hash table, this should be the hash key name; if using a hash+range table, this should be an array
		//		of both the hash and range key names.
		idProperty: 'id',

		//	queryEngine: SimpleQueryEngine
		//		Provides basic support for using DynamoDB with Observable. Not necessarily reliable since it does
		//		not account for record changes on the server or any special server sort ordering.
		queryEngine: SimpleQueryEngine,

		//	region: string
		//		The AWS region that the target DynamoDB instance is running in.
		region: null,

		//	endpointUrl: string?
		//		An optional URL to the DynamoDB endpoint. If this URL not specified, it will be generated using the
		//		region property.
		endpointUrl: null,

		//	credentials: object?
		//		An optional object containing AWS authorization credentials. Currently three properties are
		//		used:
		//			AccessKeyId: string
		//				An AWS access key, temporary or permanent.
		//			SecretAccessKey: string
		//				An AWS secret key, temporary or permanent.
		//			SessionToken: string?
		//				An optional temporary session token.
		//
		//		The credentials object uses AWS standard property names, so the Credentials property of a
		//		credentials object returned by the Security Token Service can be used directly with the DynamoDB
		//		store. More information about temporary credentials and security tokens is available at
		//		http://docs.aws.amazon.com/STS/latest/UsingSTS/Welcome.html
		credentials: null,

		_signRequest: function (/*object*/ request, /*Date?*/ date) {
			//	summary:
			//		Sign an object containing request data. The result of signing is that three additional headers
			//		will be added to the request. See http://docs.aws.amazon.com/general/latest/gr/sigv4_signing.html
			//		for more information about the signing process.
			//
			//	request: Object
			//  	Object with keys:
			//    		body: request body
			//    		host: DynamoDB host
			//    		headers: Request headers -- these will be updated
			//    		method: HTTP method
			//  date: Date
			//  	Optional time of the request. If not specified, the current time is used.
			//	returns:
			//		None

			date = toUTCString(date || new Date());
			var shortDate = date.slice(0, 8);
			var secretKey = this.credentials.SecretAccessKey;
			var accessKey = this.credentials.AccessKeyId;
			var sessionToken = this.credentials.SessionToken;
			var tokenHeader = '';
			var tokenTag = '';

			request.headers['x-amz-date'] = date;

			if (sessionToken) {
				tokenHeader = 'x-amz-security-token:' + sessionToken + '\n';
				tokenTag = ';x-amz-security-token';
				request.headers['x-amz-security-token'] = sessionToken;
			}

			var canonicalRequest = request.method + '\n/\n\n' +
				'host:' + request.host + '\n' +
				'x-amz-date:' + date + '\n' +
				tokenHeader +
				'x-amz-target:' + request.headers['x-amz-target'] + '\n' +
				'\n' +
				'host;x-amz-date' + tokenTag +
				';x-amz-target\n' + hash(request.body, digests.outputTypes.Hex);

			var key = getSigningKey(shortDate, secretKey, this.region);

			var stringToSign = 'AWS4-HMAC-SHA256\n' +
				date + '\n' +
				shortDate + '/' + this.region + '/dynamodb/aws4_request\n' +
				hash(canonicalRequest, digests.outputTypes.Hex);

			var signature = hmac(key, stringToSign, digests.outputTypes.Hex);

			request.headers.authorization = 'AWS4-HMAC-SHA256 Credential=' + accessKey + '/' + shortDate + '/' +
				this.region + '/dynamodb/aws4_request,SignedHeaders=host;x-amz-date;x-amz-target' + tokenTag +
				',Signature=' + signature;
		},

		_rpc: function (/**string*/ action, /**Object*/ data) {
			//	summary:
			//		A convenience method for sending requests to DynamoDB with improved error reporting and error
			//		retries
			//		(<http://docs.aws.amazon.com/amazondynamodb/latest/developerguide/ErrorHandling.html#APIRetries>).
			//	action:
			//		The name of the action to perform. See
			//		<http://docs.aws.amazon.com/amazondynamodb/latest/APIReference/API_Operations.html> for a list.
			//	data:
			//		The request payload.
			//	returns: dojo/promise/Promise -> Object

			var	timeoutId;
			var	requestPromise;
			var	dfd = new Deferred(function () {
				requestPromise && requestPromise.cancel.apply(request, arguments);
				clearTimeout(timeoutId);
			});
			var	headers = {
				'Content-Type': 'application/x-amz-json-1.0',
				'x-amz-target': 'DynamoDB_20120810.' + action
			};

			data.TableName = this.tableName;
			data = JSON.stringify(data);

			// if no endpoint URL has been specified, create one from the AWS region
			if (!this.endpointUrl) {
				this.endpointUrl = 'https://dynamodb.' + this.region + '.amazonaws.com';
			}

			// only sign the request if credentials were provided
			if (this.credentials) {
				this._signRequest({
					body: data,
					host: getHostName(this.endpointUrl),
					method: 'POST',
					headers: headers
				});
			}

			var endpointUrl = this.endpointUrl;
			var maxRetries = this.maxRetries;
			var	currentRetry = 0;

			(function sendRequest() {
				requestPromise = request.post(endpointUrl, {
					data: data,
					headers: headers,
					handleAs: 'json'
				}).then(lang.hitch(dfd, 'resolve'), function (error) {
					var response = error.response;

					if (++currentRetry === maxRetries) {
						dfd.reject(error);
						return;
					}

					if (response.status >= 500 || response.status < 400 ||
						(response.status === 400 && response.data && response.data.__type &&
						(response.data.__type.indexOf('#ProvisionedThroughputExceededException') > -1 ||
						 response.data.__type.indexOf('#ThrottlingException') > -1))
					) {
						timeoutId = setTimeout(sendRequest, Math.pow(2, currentRetry) * 50);
					}
					else {
						dfd.reject(error);
					}
				});
			})();

			return dfd.promise.then(function (response) {
				return response;
			}, function (error) {
				var errorInfo = error.response && error.response.data;
				if (errorInfo) {
					throw new RequestError(errorInfo.__type + ': ' + errorInfo.message, error.response);
				}

				throw error;
			});
		},

		_getKeyFromId: function (/**string|Array*/ id) {
			//	summary:
			//		Generates a DynamoDB attribute map from a DynamoDB scalar identity value.
			//	id:
			//		An opaque record identifier created by `DynamoDB#getIdentity`, or an array of primary key
			//		values.
			//	returns: Object

			var key = {};

			if (this.idProperty instanceof Array) {
				if (typeof id === 'string') {
					id = arrayUtil.map(id.split('/'), function (value) {
						var type = value.charAt(0);
						value = value.slice(1);

						if (type === 'N') {
							value = +value;
						}

						return value;
					});
				}

				arrayUtil.forEach(id, function (value, index) {
					key[this.idProperty[index]] = getDynamoValue(value);
				}, this);
			}
			else {
				key[this.idProperty] = getDynamoValue(id);
			}

			return key;
		},

		get: function (/**string|number|Array*/ id) {
			//	summary:
			//		Retrieves a single record from the table.
			//	id:
			//		The identifier for the record. If using a hash table, a scalar value corresponding to the hash key
			//		of the table. If using a hash+range table, an array of values corresponding to the keys specified
			//		in the `DynamoDB#idProperty` array, or a serialized ID in the format of `type + value + "/" +
			//		type + value` (e.g. `N1234/Sfoo`).
			//	returns: dojo/promise/Promise -> Object
			//		The object from the server, or `null` if it does not exist.

			var data = {
				Key: this._getKeyFromId(id),
				ConsistentRead: this.consistentRead
			};

			if (this.attributesToGet) {
				data.AttributesToGet = this.attributesToGet;
			}

			return this._rpc('GetItem', data).then(function (object) {
				return fromDynamoObject(object.Item);
			});
		},

		getIdentity: function (/**Object*/ object) {
			//	summary:
			//		Generates and returns an opaque scalar identifier for a given object.
			//	object:
			//		A data object from this store.
			//	returns: string

			var id;
			if (this.idProperty instanceof Array) {
				id = arrayUtil.map(this.idProperty, function (property) {
					return getDynamoType(object[property]) + object[property];
				}).join('/');
			}
			else {
				id = object[this.idProperty];
			}

			return id;
		},

		query: function (/**Object*/ query, /**Object?*/ options) {
			//	summary:
			//		Retrieves multiple records from the table. Additional restrictions exist for DynamoDB that
			//		do not exist with a standard SQL database.
			//	query:
			//		A hash map of values to query for. If the value is an array, DynamoDB will search for any of the
			//		specified values for that array. The result sets are combined using an AND operator, so all
			//		specified attributes must match for a record to be returned.
			//
			//		For a query on a table, you can only have conditions on the table primary key attributes. You can
			//		optionally specify a second condition, referring to the range key attribute.
			//
			//		For a query on a secondary index, you can only have conditions on the index key attributes. You
			//		can optionally specify a second condition, referring to the index key range attribute.
			//	options:
			//		A set of options. The following options are supported by DynamoDB, with limitations described
			//		below:
			//		* `start` (number|Object): The record to start querying from. Because DynamoDB does not support
			//		  starting a set of query results from an arbitrary numeric index, if a number is provided, all
			//		  results up to `start + count` will be retrieved on each request.
			//		* `count` (number): The number of records to retrieve. If `start` is a number, this will be
			//		  combined with `start` and used as the Limit for the request.
			//		* `indexName` (string): The name of the secondary index to query against. If this property
			//		  is not explicitly specified but a `sort` option is specified, the name of the sort attribute
			//		  will be used as the name of the index to use.
			//		* `sort` (Array): An array containing a single `{ attribute, descending }` object. Only a single
			//		  sort dimension is supported, and the attribute must match the range key of the index being
			//		  queried.
			//		* `fetchTotal` (boolean): Whether or not to retrieve the total number of available records. This
			//		  requires multiple requests to DynamoDB. Defaults to `true`.
			//		* `filter`: (Object): An object defining a DynamoDB filter expression. This object must contain
			//		the following properties:
			//			* `FilterExpression`: (string) A filter expression string. This property is required.
			//			* `ExpressionAttributeValues`: (Object) An optional mapping of value placeholder strings to
			//			  actual values. Note that value placeholders must start with ':'.
			//			* `ExpressionAttributeNames`: (Object) An optional mapping of name placeholder strings to full
			//			  name strings. This is typically used when a filter expression uses a property name that is a
			//			  DynamoDB reserved word, such as 'name'. Note that name placeholders must start with '#'.
			//		  Below is an example filter that will cause the result set to contain only items whose `name`
			//		  property contains tthe word "The".
			//		  |	filter: {
			//		  |		FilterExpression: 'contains(#n, :word)',
			//		  |		ExpressionAttributeNames: { '#n': 'name' },
			//		  |		ExpressionAttributeValues: { ':word': 'The' }
			//		  | }
			//		  For more information about DynamoDB query syntax, see:
			//		  http://docs.aws.amazon.com/amazondynamodb/latest/developerguide/QueryAndScan.html.
			//	returns: dojo/store/util/QueryResults
			//		A list of objects matching the query.

			// jshint maxcomplexity:17

			function copyFilterMap(map) {
				var values = {};
				for (var key in map) {
					values[key] = getDynamoValue(map[key]);
				}
				return values;
			}

			options = options || {};

			var data = {
				KeyConditions: {},
				ConsistentRead: this.consistentRead
			};

			if (this.attributesToGet) {
				data.AttributesToGet = this.attributesToGet;
			}

			if (options.indexName) {
				data.IndexName = options.indexName;
			}

			if (options.sort && options.sort.length) {
				if (options.sort.length > 1) {
					throw new Error('Cannot sort by more than one dimension');
				}

				if (!data.IndexName) {
					data.IndexName = options.sort[0].attribute;
				}

				data.ScanIndexForward = !options.sort[0].descending;
			}

			for (var k in query) {
				var value = query[k];

				data.KeyConditions[k] = {
					AttributeValueList: value instanceof Array ? arrayUtil.map(value, getDynamoValue) :
						[ getDynamoValue(value) ],
					ComparisonOperator: 'EQ'
				};
			}

			if (options.filter) {
				var filter = options.filter;
				data.FilterExpression = filter.FilterExpression;

				if (filter.ExpressionAttributeValues) {
					data.ExpressionAttributeValues = copyFilterMap(filter.ExpressionAttributeValues);
				}

				if (filter.ExpressionAttributeNames) {
					data.ExpressionAttributeNames = filter.ExpressionAttributeNames;
				}
			}

			var dfd = new Deferred(function () {
				request && request.cancel.apply(request, arguments);
			});
			var response = createQueryResults(dfd.promise);

			if (options.fetchTotal !== false) {
				response.total = this._rpc('Query', lang.mixin({}, data, {
					Select: 'COUNT'
				})).then(function (response) {
					return response.Count;
				});
			}

			var self = this;
			var result = [];
			var skipRecords = typeof options.start === 'number' ? options.start : 0;
			var recordsToRetrieve = (skipRecords + (options.count || 0)) || Infinity;
			var request;

			(function nextQuery(nextStartKey) {
				data.Limit = recordsToRetrieve < Infinity ? recordsToRetrieve : undefined;
				data.ExclusiveStartKey = nextStartKey;

				request = self._rpc('Query', data).then(function (response) {
					if (response.Items.length) {
						var newData = arrayUtil.map(response.Items.slice(skipRecords), fromDynamoObject);

						if (skipRecords > 0) {
							skipRecords = Math.max(skipRecords - newData.length, 0);
						}

						recordsToRetrieve -= newData.length;
						result = result.concat(newData);
					}

					// DynamoDB has a 1MB limit per request; if we have not retrieved all the requested records when the
					// limit is reached, the response will contain a LastEvaluatedKey value that can be used to continue
					// the query in a subsequent operation.
					if (recordsToRetrieve > 0 && response.LastEvaluatedKey) {
						nextQuery(response.LastEvaluatedKey);
					}
					else {
						dfd.resolve(result);
					}
				}, lang.hitch(dfd, 'reject'));
			})(typeof options.start === 'object' ? toDynamoObject(options.start) : undefined);

			return response;
		},

		remove: function (/**string|number|Array*/ id, /**Object*/ options) {
			//	summary:
			//		Removes a record from the table.
			//	id:
			//		See `DynamoDB#get` for information on the structure of the identifier. Note that you must use the
			//		serialized ID format if using a hash+range table and wrapping DynamoDB with Observable.
			//	options:
			//		Additional options for the remove operation:
			//		* `expected` (Object): The object expected to exist on the server for the given identifier. If the
			//		  expected object does not match the object on the server, the remove operation will fail.
			//	returns: dojo/promise/Promise -> Object
			//		The old object that was removed from the server, or `null` if there was no old object.

			options = options || {};

			var data = {
				Key: this._getKeyFromId(id),
				ReturnValues: 'ALL_OLD'
			};

			if (options.expected) {
				data.Expected = {};
				for (var k in options.expected) {
					data.Expected[k] = {
						Value: getDynamoValue(options.expected[k])
					};
				}
			}

			return this._rpc('DeleteItem', data).then(function (object) {
				return fromDynamoObject(object.Attributes);
			});
		},

		put: function (/**Object*/ object, /**Object?*/ options) {
			//	summary:
			//		Puts an object into the table.
			//	object:
			//		The object to put into the table.
			//	options:
			//		Additional options for the put operation:
			//		* `overwrite` (boolean): If set to `false`, and an object with the same identifier as the one
			//		  being put into the table already exists, the put will fail.
			//		* `id` (string|number|Array): An identifier that will be assigned to the object before it is put
			//		  to the store, overriding any identifier that already exists on the object. Note that you must
			//		  use the serialized ID format if using a hash+range table and wrapping DynamoDB with Observable.
			//		* `expected` (Object): The object expected to exist on the server for the given identifier. If the
			//		  expected object does not match the object on the server, the remove operation will fail.
			//	returns: dojo/promise/Promise

			options = options || {};

			var data = {
				Item: toDynamoObject(object)
			};

			if (options.id) {
				lang.mixin(data.Item, this._getKeyFromId(options.id));
			}

			if (options.overwrite === false) {
				data.Expected = {};
				var idProperties = this.idProperty instanceof Array ? this.idProperty : [ this.idProperty ];
				arrayUtil.forEach(idProperties, function (property) {
					data.Expected[property] = { Exists: false };
				});
			}
			else if (options.expected) {
				data.Expected = {};
				for (var k in options.expected) {
					data.Expected[k] = {
						Value: getDynamoValue(options.expected[k])
					};
				}
			}

			return this._rpc('PutItem', data).then(function () {
				// Ensure that no data is returned from the rpc promise chain since a returned object would override the
				// object passed to `put` by Observable
			});
		},

		add: function (/**Object*/ object, /**Object?*/ options) {
			//	summary:
			//		Puts an object to the table only if it does not already exist. See `DynamoDB#put` for more
			//		information.
			//	returns: dojo/promise/Promise
			//		See `DynamoDB#put` for more information.

			options = options || {};
			options.overwrite = false;
			return this.put(object, options);
		}
	});
});
