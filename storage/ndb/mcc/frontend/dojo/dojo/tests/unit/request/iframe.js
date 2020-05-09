define([
	'intern!object',
	'intern/chai!assert',
	'../../../request/iframe',
	'dojo/dom-construct',
	'dojo/dom',
	'dojo/domReady!'
], function (registerSuite, assert, iframe, domConstruct, dom) {
	var form;
	registerSuite({
		name: 'dojo/request/iframe',

		beforeEach: function () {
			form = domConstruct.place('<form id="contentArrayTest" method="get" enctype="multipart/form-data"><input type="hidden" name="size" value="42"/></form>', document.body);
		},

		afterEach: function () {
			domConstruct.destroy(form);
			form = null;
		},

		'.get text': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'text'
				},
				preventCache: true
			}).response.then(
				def.callback(function (response) {
					assert.strictEqual(response.data, 'iframe succeeded');
				}),
				def.reject
			);
		},

		'.post json': function () {
			var def = this.async();

			iframe.post('/__services/request/iframe', {
				form: 'contentArrayTest',
				query: {
					type: 'json'
				},
				data: {
					color: 'blue'
				},
				handleAs: 'json',
				preventCache: true
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.query.type, 'json');
					assert.strictEqual(data.payload.color, 'blue');
					assert.strictEqual(data.payload.size, '42');
				}),
				def.reject
			);
		},

		'.post no form': function () {
			var def = this.async();

			iframe.post('/__services/request/iframe', {
				query: {
					type: 'json'
				},
				data: {
					color: 'blue'
				},
				handleAs: 'json',
				preventCache: true
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.query.type, 'json');
					assert.strictEqual(data.payload.color, 'blue');
				}),
				def.reject
			);
		},

		'.get json': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'json'
				},
				data: {
					color: 'blue'
				},
				handleAs: 'json',
				preventCache: true
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.query.type, 'json');
					assert.strictEqual(data.query.color, 'blue');
				}),
				def.reject
			);
		},

		'.get with form': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				form: 'contentArrayTest',
				query: {
					type: 'json'
				},
				data: {
					color: 'blue'
				},
				handleAs: 'json',
				preventCache: true
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.query.type, 'json');
					assert.strictEqual(data.query.color, 'blue');
					assert.strictEqual(data.query.size, '42');
				}),
				def.reject
			);
		},

		'.get javascript': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'javascript'
				},
				handleAs: 'javascript',
				preventCache: true
			}).then(
				def.callback(function () {
					assert.strictEqual(window.iframeTestingFunction(), 42);
				}),
				def.reject
			);
		},

		'.get html': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'html'
				},
				handleAs: 'html',
				preventCache: true
			}).then(
				def.callback(function (data) {
					var element = data.getElementsByTagName('h1')[0];
					assert.strictEqual(element.innerHTML, 'SUCCESSFUL HTML response');
				}),
				def.reject
			);
		},

		'.get xml': function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'xml'
				},
				handleAs: 'xml',
				preventCache: true
			}).then(
				def.callback(function (data) {
					var elements = data.documentElement.getElementsByTagName('child');
					assert.strictEqual(elements.length, 4);
				}),
				def.reject
			);
		},

		'data array': function () {
			var def = this.async();

			iframe.post('/__services/request/iframe', {
				query: {
					type: 'json'
				},
				form: 'contentArrayTest',
				data: {
					tag: [ 'value1', 'value2' ]
				},
				handleAs: 'json'
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data.query.type, 'json');
					assert.strictEqual(data.payload.tag, 'value2');
					assert.strictEqual(data.payload.size, '42');

					/* Make sure the form is still in the DOM and hasn't moved */
					var form = dom.byId('contentArrayTest');
					assert.ok(form);
					assert.notStrictEqual(form.style.position, 'absolute');
				}),
				def.reject
			);
		},

		queue: function () {
			var def = this.async();

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'text',
					delay: 2000,
					text: 'one'
				}
			}).then(
				def.rejectOnError(function (data) {
					assert.strictEqual(data, 'one');
				}),
				def.reject
			);

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'text',
					text: 'two'
				}
			}).then(
				def.rejectOnError(function (data) {
					assert.strictEqual(data, 'two');
				}),
				def.reject
			);

			iframe.get('/__services/request/iframe', {
				query: {
					type: 'text',
					text: 'three'
				}
			}).then(
				def.callback(function (data) {
					assert.strictEqual(data, 'three');
				}),
				def.reject
			);

			//assert.strictEqual(iframe._dfdQueue.length, 3);
		}
	});
});
