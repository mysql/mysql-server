define([
	'intern!object',
	'intern/chai!assert',
	'../../../request/util',
	'../../../has',
	'../../../request/xhr'
], function(registerSuite, assert, util, has){
	registerSuite({
		name: 'dojo/request/util',

		'deepCopy': function(){
			var object1 = {
				apple: 0,
				banana: {
					weight: 52,
					price: 100,
					code: "B12345",
					purchased: new Date(2016, 0, 1)
				},
				cherry: 97
			};
			var object2 = {
				banana: {
					price: 200,
					code: "B98765",
					purchased: new Date(2017, 0, 1)
				},
				durian: 100
			};
			util.deepCopy(object1, object2);
			assert.strictEqual(object1.banana.weight, 52);
			assert.strictEqual(object1.banana.price, 200);
			assert.strictEqual(object1.banana.code, "B98765");
			assert.equal(object1.banana.purchased.getTime(), new Date(2017, 0, 1).getTime());
		},

		'.deepCopy should ignore the __proto__ property': function() {
			var payload = JSON.parse('{ "__proto__": { "protoPollution": true }}');
			util.deepCopy({}, payload);
			assert.isUndefined(({}).protoPollution);
		},

		'deepCopy with FormData': function(){
			if (has('native-formdata')) {
				var formData = new FormData();
				var object1 = {
					apple: 0,
					banana: {
						weight: 52,
						price: 100,
						code: "B12345",
						purchased: new Date(2016, 0, 1)
					},
					cherry: 97
				};
				var object2 = {
					banana: {
						price: 200,
						code: "B98765",
						purchased: new Date(2017, 0, 1)
					},
					formData: formData,
					durian: 100
				};
				util.deepCopy(object1, object2);
				assert.strictEqual(object1.banana.weight, 52);
				assert.strictEqual(object1.banana.price, 200);
				assert.strictEqual(object1.banana.code, "B98765");
				assert.strictEqual(object1.formData, formData);
				assert.equal(object1.banana.purchased.getTime(), new Date(2017, 0, 1).getTime());
			} else {
				this.skip('Do not run test if FormData not available.');
			}
		},

		'deepCopy with Blob': function(){
			if (has('native-blob')) {
				var blob = new Blob([JSON.stringify({test: "data"})], {type: 'application/json'});
				var object1 = {
					apple: 0,
					banana: {
						weight: 52,
						price: 100,
						code: "B12345",
						purchased: new Date(2016, 0, 1)
					},
					cherry: 97
				};
				var object2 = {
					banana: {
						price: 200,
						code: "B98765",
						purchased: new Date(2017, 0, 1)
					},
					blob: blob,
					durian: 100
				};
				util.deepCopy(object1, object2);
				assert.strictEqual(object1.banana.weight, 52);
				assert.strictEqual(object1.banana.price, 200);
				assert.strictEqual(object1.banana.code, "B98765");
				assert.strictEqual(object1.blob, blob);
				assert.equal(object1.banana.purchased.getTime(), new Date(2017, 0, 1).getTime());
			} else {
				this.skip('Do not run test if Blob not available.');
			}
		},

		'deepCopy with Element, given Element is defined': function(){
			if (typeof Element !== 'undefined') {
				var element = document.createElement('span');
				var object1 = {
					apple: 0,
					banana: {
						weight: 52,
						price: 100,
						code: "B12345",
						purchased: new Date(2016, 0, 1)
					},
					cherry: 97
				};
				var object2 = {
					banana: {
						price: 200,
						code: "B98765",
						purchased: new Date(2017, 0, 1)
					},
					element: element,
					durian: 100
				};
				util.deepCopy(object1, object2);
				assert.strictEqual(object1.banana.weight, 52);
				assert.strictEqual(object1.banana.price, 200);
				assert.strictEqual(object1.banana.code, "B98765");
				assert.strictEqual(object1.element, element);
				assert.equal(object1.banana.purchased.getTime(), new Date(2017, 0, 1).getTime());
			} else {
				this.skip('Do not run test if Element not available.');
			}
		},

		'deepCopy with Element, given Element is not defined': function(){
			if (typeof Element !== 'undefined') {
				this.skip('Do not run test if Element is available.');
			}

			var element = {nodeType: 1, value: 'Orange'};
			var object1 = {
				apple: 0,
				banana: {
					weight: 52,
					price: 100,
					code: "B12345",
					purchased: new Date(2016, 0, 1)
				},
				cherry: 97
			};
			var object2 = {
				banana: {
					price: 200,
					code: "B98765",
					purchased: new Date(2017, 0, 1)
				},
				element: element,
				durian: 100
			};
			util.deepCopy(object1, object2);
			assert.strictEqual(object1.banana.weight, 52);
			assert.strictEqual(object1.banana.price, 200);
			assert.strictEqual(object1.banana.code, "B98765");
			assert.strictEqual(object1.element, element);
			assert.equal(object1.banana.purchased.getTime(), new Date(2017, 0, 1).getTime());
		}
	});
});
